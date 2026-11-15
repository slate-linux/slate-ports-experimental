import re
import shutil
import pathlib
import subprocess

# Functions
def get_hash(link: str):
    result = subprocess.run(
        f"curl -sL {link} | sha256sum | cut -d' ' -f1",
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
    if not line.strip(): continue
    if line.startswith("#") or line.startswith(";"): continue

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
output.mkdir(exist_ok=True, parents=True)

option = input("==> would you like to remove the older files? (y/N)")
if option == "y" or option == "yes":
    shutil.rmtree(output)
    output.mkdir(exist_ok=True, parents=True)
    print("==> removed, moving on to the generation")
else:
    print("==> aborted")

for name, data in ports.items():
    portdir = output / pathlib.Path(name)
    portdir.mkdir(exist_ok=True, parents=True)

    with open(portdir / "meta.conf", "w") as file:
        content = ""
        if not data.get("version"):
            raise ValueError(f"Version not given for '{name}'")

        content += f"VERSION={data["version"]}\n"

        if not data.get("tarball"):
            raise ValueError(f"Archive (tarball) not given for '{name}'")

        print(f"==> generating hash for '{name}', this might take a while")
        content += f"HASH={get_hash(data["tarball"])}\n"

        content += "DEPENDS="
        if data.get("depends"):
            content += "\n    "
            content += "\n    ".join(data["depends"].split(" "))
        
        file.write(content)