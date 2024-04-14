dnf update -y
dnf config-manager --add-repo=https://download.docker.com/linux/centos/docker-ce.repo
dnf install docker-ce docker-ce-cli containerd.io

systemctl start docker
systemctl enable docker

usermod -aG docker $(whoami)


