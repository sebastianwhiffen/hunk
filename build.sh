gcc main.c -O2 -g -o main
codesign -s - -v -f --entitlements debug.entitlements ./main
codesign -d -vvv --entitlements - ./main
