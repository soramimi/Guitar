
PROJECT_DIR = ..

main:
	echo Hello, world $(OS) $(ARCH)

guitar-deb:
	cd $(PROJECT_DIR)/docker && make guitar-deb

ci-win: build-win deploy-win

build-win:
	cd $(PROJECT_DIR) && ruby packaging/win/build.rb

deploy-win:
	cd $(PROJECT_DIR) && ruby packaging/win/release.rb
	cd $(PROJECT_DIR) && ruby packaging/win/deploy.rb

ls:
	ls -la $(PROJECT_DIR)/packaging/deb/*.deb

prepare:
	cd $(PROJECT_DIR) && ruby prepare.rb

clean:
	ruby clean.rb
