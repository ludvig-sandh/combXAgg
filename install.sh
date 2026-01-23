#!/bin/sh
#!/bin/sh
LIST_OF_APPS="build-essential dos2unix g++ libnuma-dev make numactl parallel python3 python3-pip time zip bc"

echo "############################################"
echo "installing required ubuntu packages... $LIST_OF_APPS"
echo "############################################"

sudo apt-get update
sudo apt-get install -y $LIST_OF_APPS

LIST_PYTHON_PACKAGES="python3-numpy python3-matplotlib python3-pandas python3-seaborn python3-ipython python3-ipykernel python3-jinja2 python3-colorama"
echo "############################################"
echo "installing python3 packages... $LIST_PYTHON_PACKAGES"
echo "############################################"

sudo apt install -y $LIST_PYTHON_PACKAGES