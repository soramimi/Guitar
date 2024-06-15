
all:

build:
	docker build . -t file

run:
	docker run -it -v ./:/root/docker file

clean:
	rm -f file magic.mgc libmagic.so
