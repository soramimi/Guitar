UID := `id -u`
GID := `id -g`
UNAME := `id -un`
GNAME := `id -gn`
NAME := ubuntu-guitar-env-${UNAME}
HOMEDIR := /home/${UNAME}
SSHPORT := 65522
PASSWORD :=

PROJNAME := Guitar
PROJHOME := `realpath ../../${PROJNAME}`

main:
	@echo "  $ make build             build container"
	@echo
	@echo "daemon mode:"
	@echo
	@echo "  $ make up                start container"
	@echo "  $ make down              stop container"
	@echo "  $ make sh                login container"
	@echo
	@echo "single mode:"
	@echo
	@echo "  $ make guitar-deb        build guitar deb package"
	@echo "  $ make guitar-appimage   build guitar appimage"

clean: down
	rm -fr home

guitar-deb:
	make run RUN="make deb"

guitar-appimage:
	make run RUN="make appimage"

build:
	cd DockerBuildFiles && docker buildx build . --build-arg UNAME=${UNAME} --build-arg GNAME=${GNAME} --build-arg UID=${UID} --build-arg GID=${GID} --build-arg HOMEDIR=${HOMEDIR} -t ${NAME}

up: home home/.bashrc home/.profile 
	echo ${UNAME}:${PASSWORD} >./home/.password
	echo CONTAINER_NAME=${NAME} >./home/.container.sh
	docker run --device /dev/fuse --privileged --name ${NAME} -d -p ${SSHPORT}:${SSHPORT} -v ${PROJHOME}:/${PROJNAME} -v ./home:${HOMEDIR} -e UNAME=${UNAME} -e GNAME=${GNAME} -e UID=${UID} -e GID=${GID} -e HOMEDIR=${HOMEDIR} -e CONTAINER_NAME=${NAME} -e SSHPORT=${SSHPORT} -e COMMAND=up ${NAME}

run: down home home/.bashrc home/.profile 
	echo ${RUN} >home/run.sh
	echo CONTAINER_NAME=${NAME} >./home/.container.sh
	docker run --device /dev/fuse --privileged --name ${NAME} --rm -v ${PROJHOME}:/${PROJNAME} -v ./home:${HOMEDIR} -e UNAME=${UNAME} -e GNAME=${GNAME} -e UID=${UID} -e GID=${GID} -e HOMEDIR=${HOMEDIR} -e CONTAINER_NAME=${NAME} -e COMMAND=run ${NAME}
	
home:
	-mkdir home
	cp -a scripts/* home/

home/.bashrc:
	cp DockerBuildFiles/_bashrc ./home/.bashrc

home/.profile:
	cp DockerBuildFiles/_profile ./home/.profile

down:
	-docker kill ${NAME}
	-docker rm ${NAME}

sh:
	docker exec -it ${NAME} /usr/bin/su ${UNAME}

root:
	docker exec -it ${NAME} /bin/bash

ssh:
	ssh -XC localhost -p ${SSHPORT}

