import re
import shutil
import pathlib
import subprocess

# Constants
HASHING = "sha256"

# Templates
HEADER = """#!/bin/sh
set -eu

BUILD="${{PORTS_DIR%/:-/usr/ports}}/{port}/build"
mkdir -p $BUILD
cd $BUILD

if [ ! -f package.tar.gz ] || [ ! "$HASH" = "$({hashing}sum package.tar.gz | cut -d' ' -f1)" ]; then
    curl -fL -o package.tar.gz {url}
fi
tar -xf package.tar.gz
cd $(tar --exclude="*/*" -tf package.tar.gz")

"""

CONFFILE = """if [ -f configure ]; then
    ./configure --prefix="/usr"
fi

"""

BUILD = {

"make": """
""",
"cmake": """
""",
"meson": """
""",
"python": """
""",
"gnumake": """
""",

}

# Functions
def get_hash(link: str):
    result = subprocess.run(
        f"curl -sL {link} | {HASHING}sum | cut -d' ' -f1",
        shell=True,
        capture_output=True,
        text=True, check=True
    )

    return result.stdout[:-1]

# Reading file
with open("./ports.conf", "r") as file:
    FILE_CONTENT = file.readlines()

# Iteration
ports = {}
current_port = ""
for line in FILE_CONTENT:
    commentmatch = re.search(r"\s?[;#]", line)
    if commentmatch: line = line[:commentmatch.start()]

    if not line.strip(): continue

    if re.match(r"\[.+\]", line): # I hate regex sometimes
        current_port = line[1:-2].strip()
        continue

    options = line.split("=", 1)
    # Speaking of this
    # This will strictly be key=value
    # Without spaces next to the equal sign

    k, v = options[0].lower(), options[1].strip()
    
    if not current_port:
        raise SyntaxError("Current port not given")

    if ports.get(current_port):
        ports[current_port][k] = v
    else:
        ports[current_port] = { k:v }

# Generation
output=pathlib.Path("./ports/")

if output.exists():
    option = input("==> would you like to remove the older files? (y/N)")
    if option == "y" or option == "yes":
        shutil.rmtree(output)
        print("==> removed, moving on to the generation")
    else:
        print("==> aborted")
        
output.mkdir(exist_ok=True, parents=True)

for name, data in ports.items():
    portdir = output / pathlib.Path(name)
    portdir.mkdir(exist_ok=True, parents=True)

    with open(portdir / "meta.conf", "w") as file:
        content = ""
        if not data.get("version"):
            print(f"==> error: version not given for '{name}', skipping")
            continue
        content += f"VERSION={data["version"]}\n"
        
        if data.get("meta"):
            if not data.get("depends"):
                print(f"==> error: cannot generate meta package '{name}' without dependencies")
                continue
            content += "DEPENDS="
            if data.get("depends"):
                content += "\n    "
                content += "\n    ".join(data["depends"].split(" "))
        else:
            if not data.get("tarball"):
                print(f"==> error: archive (tarball) not given for '{name}', skipping")
                continue

            print(f"==> generating hash for '{name}', this might take a while")
            content += f"HASH={get_hash(data["tarball"])}\n"

            content += "DEPENDS="
            if data.get("depends"):
                content += "\n    "
                content += "\n    ".join(data["depends"].split(" "))
            
            if data.get("build"):
                content += f"\n    dev/{data["build"]}"
            
        file.write(content)
    
    with open(portdir / "port.sh", "w") as file:
        if data.get("meta"):
            file.write("")
            continue

        content = HEADER.format(hashing=HASHING, port=name, url=data["tarball"])
        
        if data.get("makeopts"):
            content += f"MAKEOPTS={data["makeopts"]}\n"
            content += "set -f\n"
            content += "set -- $MAKEOPTS\n\n"
        
        content += CONFFILE

        file.write(content)
