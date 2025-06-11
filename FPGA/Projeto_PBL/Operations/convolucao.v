module convolucao(
    input clk,
    input [199:0] matriz,
    input signed [199:0] kernel,
    output reg [199:0] valor
);

reg start_mul = 0;
wire done_mul;
wire signed [399:0] matriz_mult;

multiplicacao inst_multiplicacao (
    .clock(clk),
    .start(start_mul),
    .matrix_a(matriz),
    .matrix_b(kernel),
    .result_out(matriz_mult),
    .done(done_mul)
);

localparam S_IDLE       = 0;
localparam S_START_MUL  = 1;
localparam S_WAIT_MUL   = 2;
localparam S_STAGE_1    = 3;
localparam S_STAGE_2    = 4;
localparam S_STAGE_3    = 5;
localparam S_STAGE_4    = 6;
localparam S_STAGE_5    = 7;
localparam S_SATURATE   = 8;
localparam S_DONE       = 9;

reg [3:0] state = S_IDLE;

reg signed [17:0] sum_stage1 [0:11];
reg signed [18:0] sum_stage2 [0:5];
reg signed [19:0] sum_stage3 [0:2];
reg signed [20:0] sum_stage4 [0:1];
reg signed [21:0] sum_stage5;
reg signed [22:0] final_soma;

reg signed [15:0] products [0:24];
reg [7:0] saturado;

integer i;

// Extrai os 25 produtos de 16 bits do resultado da multiplicação
always @(*) begin
    for (i = 0; i < 25; i = i + 1) begin
        products[i] = matriz_mult[(i*16) +: 16];
    end
end

always @(posedge clk) begin
    case (state)
        S_IDLE: begin
            start_mul <= 1;
            state <= S_START_MUL;
        end

        S_START_MUL: begin
            // Apenas segura o start por 1 ciclo
            start_mul <= 0;
            state <= S_WAIT_MUL;
        end

        S_WAIT_MUL: begin
            if (done_mul) begin
                for (i = 0; i < 12; i = i + 1) begin
                    sum_stage1[i] <= products[2*i] + products[2*i + 1];
                end
                state <= S_STAGE_1;
            end
        end

        S_STAGE_1: begin
            for (i = 0; i < 6; i = i + 1) begin
                sum_stage2[i] <= sum_stage1[2*i] + sum_stage1[2*i + 1];
            end
            state <= S_STAGE_2;
        end

        S_STAGE_2: begin
            for (i = 0; i < 3; i = i + 1) begin
                sum_stage3[i] <= sum_stage2[2*i] + sum_stage2[2*i + 1];
            end
            state <= S_STAGE_3;
        end

        S_STAGE_3: begin
            sum_stage4[0] <= sum_stage3[0] + sum_stage3[1];
            sum_stage4[1] <= sum_stage3[2] + products[24]; // soma o restante
            state <= S_STAGE_4;
        end

        S_STAGE_4: begin
            sum_stage5 <= sum_stage4[0] + sum_stage4[1];
            state <= S_STAGE_5;
        end

        S_STAGE_5: begin
            final_soma <= sum_stage5;
            state <= S_SATURATE;
        end

        S_SATURATE: begin
            if (final_soma < 0)
                saturado <= 8'd0;
            else if (final_soma > 255)
                saturado <= 8'd255;
            else
                saturado <= final_soma[7:0];

            valor <= 200'd0;
            valor[8*12 +: 8] <= saturado;

            state <= S_DONE;
        end

        S_DONE: begin
            state <= S_IDLE;
        end
    endcase
end

endmodule
