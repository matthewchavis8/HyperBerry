# TODO
 - **SMMP Support** currently right now we only support one vcpu
 - **UART Emulation Support**: So currently right now for guest VM's we are using UART passthrough to map the guest VM IPA UART to the PA for our hypervisor's physical UART. I belive a issue down the line when we grow to support multiple guest vm's so we need to change the model to catch and emulate. So multiple guest vm's will attempt to write into what they think is their own phyiscal uart which will then get trapped and ahndled by our hypervisor gracefully.
