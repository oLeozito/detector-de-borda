module divisor(
    input wire clk_50MHz,
    input wire reset,
    output reg clk_1Hz
);

    reg [24:0] contador = 0;

    always @(posedge clk_50MHz or posedge reset) begin
        if (reset) begin
            contador <= 0;
            clk_1Hz <= 0;
        end else begin
            contador <= contador + 1;

            if (contador == 8_333_333) begin
                clk_1Hz <= ~clk_1Hz;
                contador <= 0;
            end
        end
    end

endmodule
