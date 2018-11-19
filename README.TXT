This tries to create a single optimized vhdl core for a particular program. 
The core is based on a single input and output bus. It can be synced to new IO or it can be made
to free run.

The language only accepts signed ints.
Only statements that lead to an IO out shall be kept.
