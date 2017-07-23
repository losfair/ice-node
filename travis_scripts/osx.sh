brew install xz
mkdir $HOME/ice-node-lib
wget -O $HOME/ice-node-lib/ice_core_macos.tar.xz https://github.com/losfair/IceCore/releases/download/v0.1.4/ice_core_macos.tar.xz
cd $HOME/ice-node-lib
xz -d < ice_core_macos.tar.xz | tar x
mv dist/* ./
