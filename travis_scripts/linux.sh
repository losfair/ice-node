mkdir /tmp/ice-node-lib
wget -O /tmp/ice-node-lib/ice_core_linux.tar.xz https://github.com/losfair/IceCore/releases/download/v0.1.1-all-2/ice_core_linux.tar.xz
cd /tmp/ice-node-lib
xz -d < ice_core_linux.tar.xz | tar x
mv dist/* ./
