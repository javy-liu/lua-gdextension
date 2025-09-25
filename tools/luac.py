def exists(env):
    return True


def generate(env):
    build_dir = env["build_dir"]

    env.Append(CPPPATH="lib/luasocket/src")
    luasocket_env = env.Clone()
    luasocket_env.Append(CPPPATH="lib/luasocket/src")
    luasocket = luasocket_env.StaticLibrary(
        target=f"{build_dir}/luasocket",
        source=[
                "lib/luasocket/src/compat.c",
                "lib/luasocket/src/auxiliar.c",
                "lib/luasocket/src/buffer.c",
                "lib/luasocket/src/except.c",
                "lib/luasocket/src/inet.c",
                "lib/luasocket/src/io.c",
                "lib/luasocket/src/timeout.c",
                "lib/luasocket/src/select.c",
                "lib/luasocket/src/tcp.c",
                "lib/luasocket/src/udp.c",
                "lib/luasocket/src/serial.c",
                "lib/luasocket/src/options.c",

                "lib/luasocket/src/luasocket.c",

                "lib/luasocket/src/usocket.c",

                "lib/luasocket/src/unix.c",
                "lib/luasocket/src/unixdgram.c",
                "lib/luasocket/src/unixstream.c",

                "lib/luasocket/src/mime.c",

                # "lib/luasocket/src/wsocket.c",
        ],
    )
    env.Append(LIBS=luasocket)

