"""Inyecta FIRMWARE_VERSION como macro en build_flags.

Toma el valor de la env var FIRMWARE_VERSION (la CI la pasa desde el workflow).
En builds locales, si no está definida, usa "dev-local" — así el OTA sabe que
no está compilado desde el pipeline y evita marcarse como igual a una versión
publicada.
"""
import os

Import("env")  # noqa: F821 - PlatformIO SCons

version = os.environ.get("FIRMWARE_VERSION", "dev-local")
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])  # noqa: F821
print(f"[inject_version] FIRMWARE_VERSION={version}")
