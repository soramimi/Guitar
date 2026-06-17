BINARY = dicedit

.PHONY: build run clean

build:
	go build -o $(BINARY) .

run: build
	./$(BINARY)

clean:
	rm -f $(BINARY)
