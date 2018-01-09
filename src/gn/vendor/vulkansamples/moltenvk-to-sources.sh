#!/bin/bash
set -e

cd $(git rev-parse --show-toplevel)
cd vendor/vulkansamples/submodules/Vulkan-LoaderAndValidationLayers
if [ ! -d MoltenVK ]; then
  git clone https://github.com/KhronosGroup/MoltenVK
fi
cd MoltenVK
git submodule update --init --recursive External/Vulkan-Hpp

awk '{
  if ($0 == "/* Begin PBXGroup section */") {x=1; delete f; g=0}
  if ($0 == "/* End PBXGroup section */") y=1;
  if (!x || y) next
  if (match($0, "[^/]*/\\* ")) {
    s=substr($0, RSTART+RLENGTH);
    match(s, " \\*/");
    r=substr(s, RSTART+RLENGTH);
    s=substr(s, 1, RSTART-1);
    if (r == "," && s !~ "\.h$") {
      f[g]=s;
      g += 1;
    }
  }
  if (match($0, "path = ")) {
    p=substr($0, RSTART+RLENGTH);
    if (match(p, ";$")) {
      p=substr(p, 1, RSTART-1);
    }
  }
  if ($0 ~ "};$") {
    for (i in f) {
      d = "MoltenVK/"
      if (p !~ "^\.\./") d = d "MoltenVK/"
      print d p "/" f[i];
    }
    delete f;
    g=0;
  }
}' MoltenVK/MoltenVK.xcodeproj/project.pbxproj | \
while read f; do
  if [ -f "$f" ]; then
    echo "  \"$(realpath --relative-to=. "$f")\","
  fi
done
