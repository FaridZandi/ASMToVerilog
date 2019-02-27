# ASM to Verilog Converter

A QT-based graphical tool for converting ASMs to Verilog code.


## Designing The ASM 
![Design](https://github.com/FaridZandi/asmToVerilog/raw/master/Screenshot%20from%202019-02-27%2011-48-32.png)

## Defining The Inputs and Outputs
![IO](https://github.com/FaridZandi/asmToVerilog/raw/master/Screenshot%20from%202019-02-27%2011-51-23.png)

## Generate the Code

```
module UselessASM(a, b);

output wire[4:0] a;
input reg[2:0] b;

integer state;

always @ (posedge clock)
begin
if (reset == 1'b1) begin
	 state <= 1;
end else
case (state)
1:begin
	a <= 2;;
	if ((!(b==3))) begin
		a <= 3;
	end
end
0:begin
	a <= 4;
end
endcase
end
always @ (posedge clock)
begin
if (reset == 1'b1) begin
	 state <= 1;
end else
case (state)
1:begin
	if (b==3)
		state <= 1;
	else if ((!(b==3)))
		state <= 0;
end
0:begin
		state <= 1;
end
endcase
end

endmodule
```
