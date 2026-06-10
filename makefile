.PHONY: debug
debug:
	$(MAKE) -C engine debug
	$(MAKE) -C sandbox debug
	$(MAKE) -C sandbox libcopy

.PHONY: release
release:
	$(MAKE) -C engine release
	$(MAKE) -C sandbox release
	$(MAKE) -C sandbox libcopy

.PHONY: clean
clean:
	$(MAKE) -C engine clean
	$(MAKE) -C sandbox clean
