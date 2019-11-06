#!/bin/bash
#
# This script creates a file $OUT in the current dir
# Suggested use is to create a temporary dir and then
# run the script, e.g.:
# cd your-volcano-checkout
# mkdir ../work
# cd ../work
# ../your-volcano-checkout/src/gn/cache-git.sh

set -e

# This script does not work inside a git repo
(
  while [ "$PWD" != "$HOME" -a "$PWD" != "/" ]; do
    if [ -d .git ]; then
      echo "Please run $0 outside of any git checkout"
      echo "Please read the comments in $0 for more info"
      D=$(echo "$PWD" | awk '{sub(ENVIRON["HOME"], "~");print}' )
      echo "Found a git checkout at: $D"
      exit 1
    fi
    cd ..
  done
) || exit 1

# clone volcano
git clone file://$HOME/restore/volcano
# volcano/build.cmd will try to build gn and ninja. Fake it out.
(
  cd volcano
  git submodule update --init vendor/subgn
  cd vendor/subgn
  git submodule update --init subninja
  mkdir -p out_bootstrap
  touch out_bootstrap/build.ninja
  ln -s /bin/true out_bootstrap/gn
  ln -s /bin/true subninja/ninja
)
# clone all submodules, build gn and ninja
VOLCANO_NO_OUT=1 VOLCANO_SKIP_CACHE=1 volcano/build.cmd

git_really_clean() {
  git reflog expire --expire=0
  git prune
  git prune-packed
  git gc --aggressive
  git reflog expire --expire=now --all
  git gc --prune=now
}

H=$(cd volcano && git log -1 --pretty='%H')

echo vendor/skia | while read d; do
  (
    cd volcano/$d
    SHALLOW_FILE=../../.git/modules/"${d/\//%2f}"/shallow
    if [ -f $SHALLOW_FILE ]; then
      git fetch --no-tags --unshallow
    fi
    # this can delete all branches
    git branch -D $(git branch | grep -v $(git rev-parse --abbrev-ref HEAD))
    TAGS=$(git tag | grep -E '.')
    if [ -n "$TAGS" ]; then
      git tag -d $TAGS
    fi
    # convert this repo to a shallow clone of depth 1
    git log -1 --pretty='%H' > $SHALLOW_FILE
    # prune the commit history
    git_really_clean
  ) || exit 1
done

# whitelist the paths to include in the cache
mkdir export
mkdir export/.git
mkdir export/vendor
# include volcano/.git/modules
mv volcano/.git/modules export/.git
# include volcano/vendor/skia (still needs to be cleaned some)
mv volcano/vendor/skia export/vendor

# remove everything else
rm -rf volcano
mv export volcano

# whitelist the paths from volcano/vendor/skia using the same process
cd volcano/vendor
mkdir export
mv skia/common export
mv skia/buildtools export
mv skia/third_party/externals export

# remove everything else
rm -rf skia
mv export skia

cd skia
mkdir third_party
mv externals third_party

# make all vendor/skia/third_party/externals repos shallow, depth 1 clones
( find third_party/externals -mindepth 1 -maxdepth 1
  echo buildtools
  echo common ) | while read d; do
    (
      cd "$d"
      if [ -f .git/shallow ]; then
        git fetch --no-tags --unshallow
      fi
      # since volcano/build.cmd checks out by hash, not by branch
      # this can delete all branches
      git branch -D $(git branch | grep -v $(git rev-parse --abbrev-ref HEAD))
      TAGS=$(git tag | grep -E '.')
      if [ -n "$TAGS" ]; then
        git tag -d $TAGS
      fi
      # convert this repo to a shallow clone
      git log -1 --pretty='%H' > .git/shallow
      # prune the commit history
      git_really_clean
    ) || exit 1
    # remove all checked-out files
    mv "$d"/.git no-checked-out-files
    rm -rf "$d"
    mkdir "$d"
    mv no-checked-out-files "$d"/.git
  done

echo "success"
echo ""
echo "now:"
echo "  cd volcano && tar zcf ../$H-cached.tar.gz .git vendor"
