def exists(env):
    return True


def generate(env):
    build_dir = env["build_dir"]

    env.Append(CPPPATH="luac/lpeg")
    lpeg_env = env.Clone()
    lpeg_env.Append(CPPPATH="luac/lpeg")
    lpeg = lpeg_env.StaticLibrary(
        target=f"{build_dir}/lpeg",
        source=[
                "luac/lpeg/lpcap.c",
                "luac/lpeg/lpcode.c",
                "luac/lpeg/lpcset.c",
                "luac/lpeg/lpprint.c",
                "luac/lpeg/lptree.c",
                "luac/lpeg/lpvm.c",
        ],
    )
    env.Append(LIBS=lpeg)
