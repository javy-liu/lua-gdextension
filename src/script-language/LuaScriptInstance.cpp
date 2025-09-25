/**
 * Copyright (C) 2025 Gil Barbosa Reis.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the “Software”), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <godot_cpp/classes/script.hpp>

#include "LuaScriptInstance.hpp"

#include "LuaScript.hpp"
#include "LuaScriptLanguage.hpp"
#include "LuaScriptMetadata.hpp"
#include "LuaScriptProperty.hpp"
#include "../LuaCoroutine.hpp"
#include "../LuaError.hpp"
#include "../LuaFunction.hpp"
#include "../LuaTable.hpp"
#include "../utils/VariantArguments.hpp"
#include "../utils/convert_godot_lua.hpp"
#include "../utils/function_wrapper.hpp"
#include "../utils/method_bind_impl.hpp"
#include "../utils/string_names.hpp"

namespace luagdextension {

LuaScriptInstance::LuaScriptInstance(Object *owner, Ref<LuaScript> script)
	: owner(owner)
	, script(script)
	, data(LuaScriptLanguage::get_singleton()->get_lua_state()->create_table())
{
	owner_to_instance.insert(owner, this);
	table_to_instance.insert(data->get_table().pointer(), this);

	data->get_table()[sol::metatable_key] = metatable;

	const LuaScriptMetadata& metadata = script->get_metadata();
	for (auto [name, signal] : metadata.signals) {
		data->rawset(name, Signal(owner, name));
	}
}

LuaScriptInstance::~LuaScriptInstance() {
	owner_to_instance.erase(owner);
	table_to_instance.erase(data->get_table().pointer());
}

GDExtensionBool set_func(LuaScriptInstance *p_instance, const StringName *p_name, const Variant *p_value) {
	// 1) try calling `_set`
	if (const LuaScriptMethod *_set = p_instance->script->get_metadata().methods.getptr(string_names->_set)) {
		Variant value_was_set = LuaCoroutine::invoke_lua(_set->method, Array::make(p_instance->owner, *p_name, *p_value), false);
		if (value_was_set) {
			return true;
		}
	}

	// b) try setter function from script property
	const LuaScriptProperty *property = p_instance->script->get_metadata().properties.getptr(*p_name);
	if (property && property->set_value(p_instance, *p_value)) {
		return true;
	}

	// c) try setting owner Object property
	if (ClassDB::class_set_property(p_instance->owner, *p_name, *p_value) == OK) {
		return true;
	}

	// d) set raw data
	p_instance->data->rawset(*p_name, *p_value);
	return true;
}

GDExtensionBool get_func(LuaScriptInstance *p_instance, const StringName *p_name, Variant *p_value) {
	// a) try calling `_get`
	if (const LuaScriptMethod *_get = p_instance->script->get_metadata().methods.getptr(string_names->_get)) {
		Variant value = LuaFunction::invoke_lua(_get->method, Array::make(p_instance->owner, *p_name), false);
		if (value != Variant()) {
			*p_value = value;
			return true;
		}
	}

	// b) try getter function from script property
	const LuaScriptProperty *property = p_instance->script->get_metadata().properties.getptr(*p_name);
	if (property && property->get_value(p_instance, *p_value)) {
		return true;
	}

	// c) access raw data
	if (auto data_value = p_instance->data->try_get(*p_name, true)) {
		*p_value = *data_value;
		return true;
	}

	// d) fallback to default property value, if there is one
	if (property) {
		Variant value = property->instantiate_default_value();
		p_instance->data->rawset(*p_name, value);
		*p_value = value;
		return true;
	}

	// e) for methods, return a bound Callable
	if (p_instance->script->get_metadata().methods.has(*p_name)) {
		*p_value = Callable(p_instance->owner, *p_name);
		return true;
	}

	return false;
}

GDExtensionScriptInstanceGetPropertyList get_property_list_func;
GDExtensionScriptInstanceFreePropertyList2 free_property_list_func;
GDExtensionScriptInstanceGetClassCategory get_class_category_func;

GDExtensionBool property_can_revert_func(LuaScriptInstance *p_instance, const StringName *p_name) {
	if (const LuaScriptMethod *method = p_instance->script->get_metadata().methods.getptr(string_names->_property_can_revert)) {
		Variant result = LuaFunction::invoke_lua(method->method, Array::make(p_instance->owner, *p_name), false);
		if (result) {
			return true;
		}
	}

	return false;
}

GDExtensionBool property_get_revert_func(LuaScriptInstance *p_instance, const StringName *p_name, Variant *r_ret) {
	if (const LuaScriptMethod *method = p_instance->script->get_metadata().methods.getptr(string_names->_property_get_revert)) {
		Variant result = LuaFunction::invoke_lua(method->method, Array::make(p_instance->owner, *p_name), true);
		if (LuaError *error = Object::cast_to<LuaError>(result)) {
			ERR_PRINT(error->get_message());
		}
		else {
			*r_ret = result;
			return true;
		}
	}

	return false;
}

Object *get_owner_func(LuaScriptInstance *p_instance) {
	return p_instance->owner;
}

void get_property_state_func(LuaScriptInstance *p_instance, GDExtensionScriptInstancePropertyStateAdd p_add_func, void *p_userdata) {
	for (Variant key : *p_instance->data.ptr()) {
		StringName name = key;
		Variant value = p_instance->data->get(key);
		p_add_func(&name, &value, p_userdata);
	}
}

GDExtensionScriptInstanceGetMethodList get_method_list_func;
GDExtensionScriptInstanceFreeMethodList2 free_method_list_func;

GDExtensionVariantType get_property_type_func(LuaScriptInstance *p_instance, const StringName *p_name, GDExtensionBool *r_is_valid) {
	if (const LuaScriptProperty *property = p_instance->script->get_metadata().properties.getptr(*p_name)) {
		*r_is_valid = true;
		return (GDExtensionVariantType) property->type;
	}
	else {
		*r_is_valid = false;
		return GDEXTENSION_VARIANT_TYPE_NIL;
	}
}

GDExtensionBool validate_property_func(LuaScriptInstance *p_instance, GDExtensionPropertyInfo *p_property) {
	if (const LuaScriptMethod *_validate_property = p_instance->script->get_metadata().methods.getptr(string_names->_validate_property)) {
		PropertyInfo property_info(p_property);
		Dictionary property_info_dict = property_info;
		LuaFunction::invoke_lua(_validate_property->method, Array::make(p_instance->owner, property_info_dict), false);
		return true;
	}
	else {
		return false;
	}
}

GDExtensionBool has_method_func(LuaScriptInstance *p_instance, const StringName *p_name) {
	return p_instance->script->_has_method(*p_name);
}

GDExtensionInt get_method_argument_count_func(LuaScriptInstance *p_instance, const StringName *p_name, GDExtensionBool *r_is_valid) {
	Variant result = p_instance->script->_get_script_method_argument_count(*p_name);
	*r_is_valid = result.get_type() != Variant::Type::NIL;
	return result;
}

void call_func(LuaScriptInstance *p_instance, const StringName *p_method, const Variant **p_args, GDExtensionInt p_argument_count, Variant *r_return, GDExtensionCallError *r_error) {
	if (const LuaScriptMethod *method = p_instance->script->get_metadata().methods.getptr(*p_method)) {
		r_error->error = GDEXTENSION_CALL_OK;
		*r_return = LuaCoroutine::invoke_lua(method->method, VariantArguments(p_instance->owner, p_args, p_argument_count), false);
	}
	else {
		r_error->error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
	}
}

void notification_func(LuaScriptInstance *p_instance, int32_t p_what, GDExtensionBool p_reversed) {
	if (const LuaScriptMethod *_notification = p_instance->script->get_metadata().methods.getptr(string_names->_notification)) {
		LuaCoroutine::invoke_lua(_notification->method, Array::make(p_instance->owner, p_what, p_reversed), false);
	}
}

void to_string_func(LuaScriptInstance *p_instance, GDExtensionBool *r_is_valid, String *r_out) {
	if (const LuaScriptMethod *_to_string = p_instance->script->get_metadata().methods.getptr(string_names->_to_string)) {
		Variant result = LuaFunction::invoke_lua(_to_string->method, Array::make(p_instance->owner), false);
		if (result) {
			*r_out = result;
			*r_is_valid = true;
		}
		else {
			*r_is_valid = false;
		}
	}
	else {
		*r_is_valid = false;
	}
}

void refcount_incremented_func(LuaScriptInstance *instance) {
}

GDExtensionBool refcount_decremented_func(LuaScriptInstance *instance) {
	return true;
}

void *get_script_func(LuaScriptInstance *instance) {
	return instance->script.ptr()->_owner;
}

GDExtensionBool is_placeholder_func(LuaScriptInstance *instance) {
	return false;
}

GDExtensionScriptInstanceSet set_fallback_func;
GDExtensionScriptInstanceGet get_fallback_func;

void *get_language_func(LuaScriptInstance *instance) {
	return LuaScriptLanguage::get_singleton()->_owner;
}

void free_func(LuaScriptInstance *instance) {
	memdelete(instance);
}

GDExtensionScriptInstanceInfo3 script_instance_info = {
	(GDExtensionScriptInstanceSet) set_func,
	(GDExtensionScriptInstanceGet) get_func,
	(GDExtensionScriptInstanceGetPropertyList) get_property_list_func,
	(GDExtensionScriptInstanceFreePropertyList2) free_property_list_func,
	(GDExtensionScriptInstanceGetClassCategory) get_class_category_func,
	(GDExtensionScriptInstancePropertyCanRevert) property_can_revert_func,
	(GDExtensionScriptInstancePropertyGetRevert) property_get_revert_func,
	(GDExtensionScriptInstanceGetOwner) get_owner_func,
	(GDExtensionScriptInstanceGetPropertyState) get_property_state_func,
	(GDExtensionScriptInstanceGetMethodList) get_method_list_func,
	(GDExtensionScriptInstanceFreeMethodList2) free_method_list_func,
	(GDExtensionScriptInstanceGetPropertyType) get_property_type_func,
	(GDExtensionScriptInstanceValidateProperty) validate_property_func,
	(GDExtensionScriptInstanceHasMethod) has_method_func,
	(GDExtensionScriptInstanceGetMethodArgumentCount) get_method_argument_count_func,
	(GDExtensionScriptInstanceCall) call_func,
	(GDExtensionScriptInstanceNotification2) notification_func,
	(GDExtensionScriptInstanceToString) to_string_func,
	(GDExtensionScriptInstanceRefCountIncremented) refcount_incremented_func,
	(GDExtensionScriptInstanceRefCountDecremented) refcount_decremented_func,
	(GDExtensionScriptInstanceGetScript) get_script_func,
	(GDExtensionScriptInstanceIsPlaceholder) is_placeholder_func,
	(GDExtensionScriptInstanceSet) set_fallback_func,
	(GDExtensionScriptInstanceGet) get_fallback_func,
	(GDExtensionScriptInstanceGetLanguage) get_language_func,
	(GDExtensionScriptInstanceFree) free_func,
};
GDExtensionScriptInstanceInfo3 *LuaScriptInstance::get_script_instance_info() {
	return &script_instance_info;
}

LuaScriptInstance *LuaScriptInstance::attached_to_object(Object *owner) {
	if (LuaScriptInstance **ptr = owner_to_instance.getptr(owner)) {
		return *ptr;
	}
	else {
		return nullptr;
	}
}

LuaState *LuaScriptInstance::get_lua_state() const {
	return data->get_lua_state();
}

static Variant _rawget(const Variant& self, const Variant& index) {
	if (LuaScriptInstance *instance = LuaScriptInstance::attached_to_object(self)) {
		return instance->data->rawget(index);
	}
	else {
		return {};
	}
}

static void _rawset(const Variant& self, const Variant& index, const Variant& value) {
	if (LuaScriptInstance *instance = LuaScriptInstance::attached_to_object(self)) {
		instance->data->rawset(index, value);
	}
}

static int lua_index(lua_State *L) {
	sol::stack_table self(L, 1);
	sol::stack_object key(L, 2);

	if (key.get_type() == sol::type::string) {
		if (LuaScriptInstance *instance = LuaScriptInstance::find_instance(self)) {
			std::string key_str = key.as<std::string>();
			String godot_key = String::utf8(key_str.c_str(), key_str.size());
			StringName key_name(godot_key);
			if (instance->owner->has_method(key_name)) {
				sol::stack::push(L, LuaScriptInstanceMethodBind(instance, key_name));
			}
			else {
				Variant value = instance->owner->get(key_name);
				lua_push(L, value);
			}
			return 1;
		}
	}
	return 0;
}

static int lua_newindex(lua_State *L) {
	sol::stack_table self(L, 1);
	sol::stack_object key(L, 2);
	sol::stack_object value(L, 3);

	if (key.get_type() == sol::type::string) {
		if (LuaScriptInstance *instance = LuaScriptInstance::find_instance(self)) {
			StringName key_name = key.as<StringName>();
			Variant value_var = to_variant(value);
			set_func(instance, &key_name, &value_var);
			return 0;
		}
	}
	self.raw_set(key, value);
	return 0;
}

static int lua_to_string(lua_State *L) {
	sol::stack_table self(L, 1);

	if (LuaScriptInstance *instance = LuaScriptInstance::find_instance(self)) {
		sol::stack::push(L, instance->owner->to_string());
		return 1;
	}
	return 0;
}

void LuaScriptInstance::register_lua(lua_State *L) {
	sol::state_view state(L);
	metatable = state.create_table_with(
		"__index", lua_index,
		"__newindex", lua_newindex,
		"__tostring", lua_to_string,
		"__metatable", "LuaScriptInstance"
	);
	rawget = wrap_function(L, _rawget);
	rawset = wrap_function(L, _rawset);
	LuaScriptInstanceMethodBind::register_usertype(state);
}

void LuaScriptInstance::unregister_lua(lua_State *L) {
	metatable = {};
	rawget = {};
	rawset = {};
}

HashMap<Object *, LuaScriptInstance *> LuaScriptInstance::owner_to_instance;
HashMap<const void *, LuaScriptInstance *> LuaScriptInstance::table_to_instance;
sol::table LuaScriptInstance::metatable;
sol::protected_function LuaScriptInstance::rawget;
sol::protected_function LuaScriptInstance::rawset;

}
