docker pull maeeen.ddns.net:5002/qemu
docker run --name qemu --mount type=bind,source=$(Join-Path -Path $pwd -ChildPath "code"),target=/work -d -it maeeen.ddns.net:5002/qemu /bin/zsh
