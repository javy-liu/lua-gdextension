def exists(env):
    return True


def generate(env):
    build_dir = env["build_dir"]

    env.Append(CPPPATH="luac/sproto")
    sproto_env = env.Clone()
    sproto_env.Append(CPPPATH="luac/sproto")
    sproto = sproto_env.StaticLibrary(
        target=f"{build_dir}/sproto",
        source=[
                "luac/sproto/sproto.c",
                "luac/sproto/lsproto.c",   # Lua 绑定
        ],
    )
    env.Append(LIBS=sproto)

