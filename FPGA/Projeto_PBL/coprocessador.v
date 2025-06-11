module coprocessador(
    input clk,
    input start,                      // Sinal de inicialização. (Inutilizado até então).
    input [2:0] tamanho,              // Tamanho de 2x2 a 5x5. 
    input [2:0] op,                   // Opcode das operações.
    input [199:0] matriz1,            // Primeira matriz.
    input [199:0] matriz2,            // Segunda matriz.

    output reg [199:0] matrizresult,  // Matriz resultado. 
    output reg overflow,              // Sinal de overflow detectado.
    output reg done                   // Indica que o processamento foi completo.
);

    // Definição dos estados.
    parameter ESPERA = 2'b00;
    parameter EXECUTA = 2'b01;
    parameter FINALIZA = 2'b10;

    reg [1:0] estado_atual, proximo_estado;

    // Fios internos para operações
	 wire [199:0] matriz_conv;

	 
    // Transição de estado (sincronizada com clock)
    always @(posedge clk) begin
        estado_atual <= proximo_estado;
    end

	 
    // Lógica de próximo estado
    always @(*) begin
        case (estado_atual)
            ESPERA: begin
                if (start)
                    proximo_estado = EXECUTA;
                else
                    proximo_estado = ESPERA;
            end
            EXECUTA: proximo_estado = FINALIZA;
            FINALIZA: proximo_estado = ESPERA;
            default: proximo_estado = ESPERA;
        endcase
    end

    // Lógica de saída (sincrona)
    always @(posedge clk) begin
        case (estado_atual)
            ESPERA: begin
                matrizresult <= 0;
                overflow <= 0;
                done <= 0;
            end

            EXECUTA: begin
                case (op)
                    3'b101: begin // Convolução
                        matrizresult <= matriz_conv;
                        overflow <= 0;
                    end
                    3'b111: begin // Reset
                        matrizresult <= 0;
                        overflow <= 0;
                    end
                    default: begin
                        matrizresult <= 0;
                        overflow <= 0;
                    end
                endcase
            end

            FINALIZA: begin
                done <= 1;
            end
        endcase
    end

    // Instâncias das operações
//	 
//	 // SOMA
//    MatrixAdder adder_inst (
//        .matrix_A(matriz1),
//        .matrix_B(matriz2),
//        .matrix_size(2'b11),
//        .result_out(matriz_add),
//        .overflow(ovf_add)
//    );
//	 
//	 // SUBTRAÇÃO
//    MatrixSubtractor subtractor_inst (
//        .matrix_A(matriz1),
//        .matrix_B(matriz2),
//        .matrix_size(2'b11),
//        .result_out(matriz_sub),
//        .overflow(ovf_sub)
//    );
//
//	 // MULTIPLICAÇÃO
//    multiplicacao multiplier_inst (
//        .matrix_a(matriz1),
//        .matrix_b(matriz2),
//        .result_out(matriz_mult),
//        .overflow_flag(ovf_mult)
//    );
//
//	 // MULTIPLICAÇÃO POR INTEIRO
//    multiplicacao_num_matriz scalar_mult_inst (
//        .matriz_A(matriz1),
//        .num_inteiro(matriz2[7:0]),
//        .matrix_size(2'b11),
//        .nova_matriz_A(matriz_multint),
//        .overflow_flag(ovf_multint)
//    );
//
//	 // OPOSTA
//    oposicao_matriz opposite_inst (
//        .matrix_A(matriz1),
//        .matrix_size(2'b11),
//        .m_oposta_A(matriz_oposta)
//    );
//
//	 // TRANSPOSTA
//    transposicao_matriz transpose_inst (
//        .matrix_A(matriz1),
//        .matrix_size(2'b11),
//        .m_transposta_A(matriz_transp)
//    );
	 
	 
	 // CONVOLUCAO
	 convolucao conv_inst (
		  .clk(clk),
		  .matriz(matriz1),
		  .kernel(matriz2),
		  .valor(matriz_conv)
	 );

endmodule
