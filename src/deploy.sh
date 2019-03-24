#!/bin/bash

echo -e "\033[0;32mDeploying updates to GitHub...\033[0m"

# Build
hugo -s ./hugo -d ./hugo/public

# Publish
cd public

git add .

# Run with argument "Your optional commit message" to use that as $msg
msg="rebuilding site `date`"
if [ $# -eq 1 ]
  then msg="$1"
fi
git commit -m "$msg"

git push origin master

# Return
cd ..