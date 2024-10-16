echo "The patch file has not been updated yet." # TODO: Update the patch file
echo "Copying QEMU"
cp -r ../../qemu/ ./qemu
cd qemu
rm -rf ./build
rm -rf ./.git
rm -rf ./.gitattributes
rm -rf ./.gitmodules
echo "Applying patch"
git apply ../inscount.patch
echo "Configuring QEMU"
mkdir build
cd build
../configure --target-list=x86_64-linux-user
echo "Building QEMU"
make clean # to be sure
make -j 32