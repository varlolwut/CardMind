#!/usr/bin/env python3
"""Prepare the official libssh2 source tree as an Arduino library."""

from pathlib import Path
import shutil
import sys


SOURCES = (
    "agent.c",
    "bcrypt_pbkdf.c",
    "channel.c",
    "comp.c",
    "chacha.c",
    "cipher-chachapoly.c",
    "crypt.c",
    "crypto.c",
    "global.c",
    "hostkey.c",
    "keepalive.c",
    "kex.c",
    "knownhost.c",
    "mac.c",
    "misc.c",
    "packet.c",
    "pem.c",
    "poly1305.c",
    "publickey.c",
    "scp.c",
    "session.c",
    "sftp.c",
    "transport.c",
    "userauth.c",
    "userauth_kbd_packet.c",
    "version.c",
)

PRIVATE_HEADERS = (
    "chacha.h",
    "channel.h",
    "cipher-chachapoly.h",
    "comp.h",
    "crypto.h",
    "crypto_config.h",
    "libssh2_priv.h",
    "libssh2_setup.h",
    "mac.h",
    "mbedtls.h",
    "misc.h",
    "packet.h",
    "poly1305.h",
    "session.h",
    "sftp.h",
    "transport.h",
    "userauth.h",
    "userauth_kbd_packet.h",
)

PUBLIC_HEADERS = (
    "libssh2.h",
    "libssh2_publickey.h",
    "libssh2_sftp.h",
)


def require_file(path: Path) -> None:
    if not path.is_file():
        raise FileNotFoundError(f"Required libssh2 file is missing: {path}")


def prepare(source_root: Path, output_root: Path) -> None:
    if str(source_root).casefold() == str(output_root).casefold():
        raise ValueError("Source and output directories must be different")
    source_dir = source_root / "src"
    include_dir = source_root / "include"
    destination = output_root / "src"
    require_file(source_root / "COPYING")
    for name in SOURCES + PRIVATE_HEADERS:
        require_file(source_dir / name)
    for name in PUBLIC_HEADERS:
        require_file(include_dir / name)

    if output_root.exists():
        shutil.rmtree(output_root)
    destination.mkdir(parents=True)

    for name in SOURCES + PRIVATE_HEADERS:
        shutil.copy2(source_dir / name, destination / name)
    for name in PUBLIC_HEADERS:
        shutil.copy2(include_dir / name, destination / name)

    shutil.copy2(source_dir / "mbedtls.c", destination / "mbedtls.inc")
    shutil.copy2(source_dir / "blowfish.c", destination / "blowfish.inc")
    shutil.copy2(source_dir / "agent_win.c", destination / "agent_win.inc")
    shutil.copy2(source_root / "COPYING", output_root / "COPYING")

    crypto_path = destination / "crypto.c"
    crypto_path.write_text(
        crypto_path.read_text(encoding="utf-8").replace(
            '#include "mbedtls.c"', '#include "mbedtls.inc"'
        ),
        encoding="utf-8",
        newline="\n",
    )
    bcrypt_path = destination / "bcrypt_pbkdf.c"
    bcrypt_path.write_text(
        bcrypt_path.read_text(encoding="utf-8").replace(
            '#include "blowfish.c"', '#include "blowfish.inc"'
        ),
        encoding="utf-8",
        newline="\n",
    )
    agent_path = destination / "agent.c"
    agent_path.write_text(
        agent_path.read_text(encoding="utf-8").replace(
            '#include "agent_win.c"', '#include "agent_win.inc"'
        ),
        encoding="utf-8",
        newline="\n",
    )

    setup_path = destination / "libssh2_setup.h"
    setup_text = setup_path.read_text(encoding="utf-8")
    setup_text = setup_text.replace(
        "#define LIBSSH2_SETUP_H\n",
        "#define LIBSSH2_SETUP_H\n\n"
        "#if defined(ARDUINO_ARCH_ESP32)\n"
        "#define LIBSSH2_MBEDTLS\n"
        "#define HAVE_UNISTD_H\n"
        "#define HAVE_INTTYPES_H\n"
        "#define HAVE_SYS_TIME_H\n"
        "#define HAVE_SYS_UIO_H\n"
        "#define HAVE_SYS_SOCKET_H\n"
        "#define HAVE_SYS_SELECT_H\n"
        "#define HAVE_SYS_IOCTL_H\n"
        "#define HAVE_GETTIMEOFDAY\n"
        "#define HAVE_STRTOLL\n"
        "#define HAVE_SNPRINTF\n"
        "#define HAVE_SELECT\n"
        "#define HAVE_O_NONBLOCK\n"
        "#endif\n",
    )
    setup_path.write_text(setup_text, encoding="utf-8", newline="\n")

    private_path = destination / "libssh2_priv.h"
    private_text = private_path.read_text(encoding="utf-8")
    private_text = private_text.replace(
        "#define MAX_SSH_PACKET_LEN 35000",
        "#define MAX_SSH_PACKET_LEN 24576",
    ).replace(
        "unsigned char buf[PACKETBUFSIZE];",
        "unsigned char *buf;",
    ).replace(
        "unsigned char outbuf[MAX_SSH_PACKET_LEN]; /* area for the outgoing data */",
        "unsigned char *outbuf; /* separately allocated outgoing data */",
    )
    private_path.write_text(private_text, encoding="utf-8", newline="\n")

    public_path = destination / "libssh2.h"
    public_text = public_path.read_text(encoding="utf-8").replace(
        "#define LIBSSH2_CHANNEL_PACKET_DEFAULT  32768",
        "#define LIBSSH2_CHANNEL_PACKET_DEFAULT  16384",
    )
    public_path.write_text(public_text, encoding="utf-8", newline="\n")

    session_path = destination / "session.c"
    session_text = session_path.read_text(encoding="utf-8")
    session_text = session_text.replace(
        "session->packet_read_timeout = LIBSSH2_DEFAULT_READ_TIMEOUT;",
        "session->packet_read_timeout = LIBSSH2_DEFAULT_READ_TIMEOUT;\n"
        "        session->packet.buf = LIBSSH2_ALLOC(session, PACKETBUFSIZE);\n"
        "        session->packet.outbuf = LIBSSH2_ALLOC(session, MAX_SSH_PACKET_LEN);\n"
        "        if(session->packet.buf == NULL || session->packet.outbuf == NULL) {\n"
        "            if(session->packet.buf != NULL)\n"
        "                LIBSSH2_FREE(session, session->packet.buf);\n"
        "            if(session->packet.outbuf != NULL)\n"
        "                LIBSSH2_FREE(session, session->packet.outbuf);\n"
        "            local_free(session, &abstract);\n"
        "            return NULL;\n"
        "        }",
        1,
    ).replace(
        "    LIBSSH2_FREE(session, session);\n\n    return 0;",
        "    LIBSSH2_FREE(session, session->packet.buf);\n"
        "    LIBSSH2_FREE(session, session->packet.outbuf);\n"
        "    LIBSSH2_FREE(session, session);\n\n    return 0;",
        1,
    )
    session_path.write_text(session_text, encoding="utf-8", newline="\n")

    (output_root / "library.properties").write_text(
        "name=CardMind libssh2\n"
        "version=1.11.1\n"
        "author=The libssh2 project and contributors\n"
        "maintainer=CardMind contributors\n"
        "sentence=Reproducible Arduino packaging of official libssh2.\n"
        "paragraph=Client-side SSH2 with the ESP32 mbedTLS backend.\n"
        "category=Communication\n"
        "url=https://libssh2.org/\n"
        "architectures=esp32\n"
        "includes=libssh2.h\n",
        encoding="utf-8",
        newline="\n",
    )


def main(arguments: list[str]) -> int:
    if len(arguments) != 3:
        raise ValueError("Usage: prepare_libssh2.py SOURCE_ROOT OUTPUT_ROOT")
    prepare(Path(arguments[1]).resolve(), Path(arguments[2]).resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
