mkdir /tmp/ice-node-lib
wget -O /tmp/ice-node-lib/ice_core_linux.tar.xz https://github.com/losfair/IceCore/releases/download/v0.2.0/ice_core_linux.tar.xz
cd /tmp/ice-node-lib
xz -d < ice_core_linux.tar.xz | tar x
mv dist/* ./
