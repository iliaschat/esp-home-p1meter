# PlatformIO extra script (loaded via "extra_scripts" in platformio.ini).
# Adds "P1 Meter:" custom targets to each environment. Each target runs
# scripts/build.sh for the firmware and device encoded in the environment
# name: <firmware>-<device>, for example public-wemos-d1-mini.

Import("env")

from SCons.Script import COMMAND_LINE_TARGETS

firmware, _, device = env["PIOENV"].partition("-")
build_script = '"{}/scripts/build.sh"'.format(env["PROJECT_DIR"])

# PlatformIO refuses a plain build without sources unless a target is given.
if not COMMAND_LINE_TARGETS:
    COMMAND_LINE_TARGETS.append("p1-compile")

# The native link step runs the ESPHome compile; the program file is an upload script.
env.Replace(
    LINKCOM=[
        "bash {} {} {} compile".format(build_script, firmware, device),
        "printf '#!/bin/sh\\nexec bash {} {} {} run\\n' > $TARGET && chmod +x $TARGET".format(
            build_script, firmware, device
        ),
    ]
)
env.AlwaysBuild("$BUILD_DIR/$PROGNAME")

TARGETS = [
    ("p1-compile", "P1 Meter: Compile", "compile"),
    ("p1-upload", "P1 Meter: Upload (compile and flash)", "run"),
    ("p1-logs", "P1 Meter: Show device logs", "logs"),
    ("p1-validate", "P1 Meter: Validate config", "config"),
    ("p1-clean", "P1 Meter: Clean", "clean"),
]

for name, title, action in TARGETS:
    env.AddCustomTarget(
        name=name,
        dependencies=None,
        actions=["bash {} {} {} {}".format(build_script, firmware, device, action)],
        title=title,
        description="{} {} ({})".format(firmware, device, action),
        always_build=True,
    )
