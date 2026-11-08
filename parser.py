import re

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

    k, v = options[0], options[1].strip()
    
    if not current_port:
        raise SyntaxError("Current port not given")

    if ports.get(current_port):
        ports[current_port][k] = v
    else:
        ports[current_port] = { k:v }
