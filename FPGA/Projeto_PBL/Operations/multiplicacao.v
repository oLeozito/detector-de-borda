module multiplicacao (
    input wire clock,
    input wire start,
    input [199:0] matrix_a,          // 25 valores de 8 bits (imagem, unsigned)
    input signed [199:0] matrix_b,   // 25 valores de 8 bits (kernel, signed)
    output reg signed [399:0] result_out,  // 25 valores de 16 bits
    output reg done
);

    reg [4:0] index;
    reg [1:0] state;

    reg [7:0] a_elem;
    reg signed [7:0] b_elem;
    reg signed [15:0] products [0:24];

    localparam S_IDLE = 2'd0;
    localparam S_CALC = 2'd1;
    localparam S_DONE = 2'd2;

    integer j;

    always @(posedge clock) begin
        case (state)
            S_IDLE: begin
                done <= 0;
                result_out <= 0;
                index <= 0;

                if (start) begin
                    state <= S_CALC;
                end else begin
                    state <= S_IDLE;
                end
            end

            S_CALC: begin
                a_elem = matrix_a[(index * 8) +: 8];
                b_elem = matrix_b[(index * 8) +: 8];

                products[index] <= $signed(a_elem) * b_elem;

                if (index == 5'd24) begin
                    state <= S_DONE;
                end

                index <= index + 1;
            end

            S_DONE: begin
                for (j = 0; j < 25; j = j + 1) begin
                    result_out[(j * 16) +: 16] <= products[j];
                end
                done <= 1;
                state <= S_IDLE;
            end
        endcase
    end

endmodule
