# Reading ISO11784/11785

Reading through specifications and some very few sample implementations I found did not really save me from testing around a LOT with RFID.

The best documentation on the suject I found was actually this [paper](http://www.blazeroc.com/Projects/Projet_kuvat/RFIDReader/RFID%20reader%20manual.pdf) by P.Viljakainen.

A lot of testing has been done with my own circuitry, a self-coiled antenna and the given pseudocode. While I was able to make it run, it was heavily depending on timing and the actual frequency of the antenna/sensor.

The second implementation now uses interrupts on both clock and data signal coming from EM4095 to count clock cycles as described in the document by P.Viljakainen. This seems to be a solid solution, more robust to inaccurate frequencies. 
The goal shoud still be to match the frequency as best as possible to achieve the best possible reading range.

The whole RFID functionality is happening in OCFRFID.[h|cpp].