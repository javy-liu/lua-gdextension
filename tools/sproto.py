def exists(env):
    return True


def generate(env):
    env.Append(CPPPATH="lib/sproto")
    # env.Append(CPPPATH="lib/luasocket/src")


    sproto_sources = [

        "lib/lpeg/lpcap.c",
        "lib/lpeg/lpcode.c",
        "lib/lpeg/lpcset.c",
        "lib/lpeg/lpprint.c",
        "lib/lpeg/lptree.c",
        "lib/lpeg/lpvm.c",

        "lib/sproto/sproto.c",
        "lib/sproto/lsproto.c",   # Lua 绑定

        # "lib/luasocket/src/compat.c",
        # "lib/luasocket/src/auxiliar.c",
        # "lib/luasocket/src/buffer.c",
        # "lib/luasocket/src/except.c",
        # "lib/luasocket/src/inet.c",
        # "lib/luasocket/src/io.c",
        # "lib/luasocket/src/timeout.c",
        # "lib/luasocket/src/select.c",
        # "lib/luasocket/src/tcp.c",
        # "lib/luasocket/src/udp.c",
        # "lib/luasocket/src/serial.c",
        # "lib/luasocket/src/options.c",
        #
        # "lib/luasocket/src/luasocket.c",
        #
        # "lib/luasocket/src/usocket.c",
        #
        # "lib/luasocket/src/unix.c",
        # "lib/luasocket/src/unixdgram.c",
        # "lib/luasocket/src/unixstream.c",

        # "lib/luasocket/src/mime.c",

        # "lib/luasocket/src/wsocket.c",
    ]
    if "SPROTO_SOURCES" not in env:
        env["SPROTO_SOURCES"] = []
    env["SPROTO_SOURCES"].extend(sproto_sources)

