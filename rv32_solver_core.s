# Mini-Rubik: change input_state to any valid 14-character state.
# Build with: make rv32_solver.s rv32_solver_gui.s
.text

start:
    # Parse seven distinct cubie digits into pieces[].
    la t0, input_state
    la t1, pieces
    li t2, 7
    li t3, 0                      # seen-cubie bitset
parse_pieces:
    lbu t4, 0(t0)
    addi t5, t4, -49              # digit '1' -> 0
    sltiu t6, t5, 7
    beqz t6, invalid_input
    li a3, 1
    sll a3, a3, t5
    and a4, t3, a3
    bnez a4, invalid_input
    or t3, t3, a3
    sb t5, 0(t1)
    addi t0, t0, 1
    addi t1, t1, 1
    addi t2, t2, -1
    bnez t2, parse_pieces

    # Rank the first six orientation digits in base 3; check total twist.
    la t1, twists
    li t2, 7
    li t3, 0                      # sum of seven twists
    li s1, 0                      # orientation rank
parse_twists:
    lbu t4, 0(t0)
    addi t5, t4, -49
    sltiu t6, t5, 3
    beqz t6, invalid_input
    sb t5, 0(t1)
    add t3, t3, t5
    li t6, 1
    beq t2, t6, last_twist
    slli a3, s1, 1
    add s1, s1, a3
    add s1, s1, t5
last_twist:
    addi t0, t0, 1
    addi t1, t1, 1
    addi t2, t2, -1
    bnez t2, parse_twists
    lbu t4, 0(t0)
    bnez t4, invalid_input
twist_mod3:
    sltiu t4, t3, 3
    bnez t4, twist_checked
    addi t3, t3, -3
    j twist_mod3
twist_checked:
    bnez t3, invalid_input

    # Lehmer rank: each digit counts smaller cubies to its right.
    la t0, pieces
    li t1, 7                      # factor: 7, 6, ..., 1
    li s0, 0                      # permutation rank
rank_piece:
    lbu t2, 0(t0)
    addi t3, t0, 1
    addi t4, t1, -1
    li t5, 0                      # Lehmer digit
rank_compare:
    beqz t4, rank_multiply
    lbu t6, 0(t3)
    sltu a3, t6, t2
    add t5, t5, a3
    addi t3, t3, 1
    addi t4, t4, -1
    j rank_compare
rank_multiply:
    li a3, 0                      # s0 * factor, without M extension
    mv a4, t1
rank_add:
    add a3, a3, s0
    addi a4, a4, -1
    bnez a4, rank_add
    add s0, a3, t5
    addi t0, t0, 1
    addi t1, t1, -1
    bnez t1, rank_piece

    # Iterative deepening A*: h = max(permutation distance, orientation distance).
    la s4, p_stack
    la s5, o_stack
    la s6, next_stack
    la s7, path
    la s8, perm_distance
    la s9, ori_distance
    la s10, perm_next_rows
    la s11, ori_next_rows
    add t0, s8, s0
    lbu s2, 0(t0)
    add t0, s9, s1
    lbu t1, 0(t0)
    bgeu s2, t1, search_bound
    mv s2, t1

search_bound:
    sh s0, 0(s4)
    sh s1, 0(s5)
    sb zero, 0(s6)
    li s3, 0                      # depth

search_node:
    add t0, s6, s3
    lbu t1, 0(t0)
    bnez t1, search_choice
    slli t2, s3, 1
    add t3, s4, t2
    lhu t4, 0(t3)
    add t3, s5, t2
    lhu t5, 0(t3)
    add t6, s8, t4
    lbu t6, 0(t6)
    add a3, s9, t5
    lbu a3, 0(a3)
    bgeu t6, a3, heuristic_ready
    mv t6, a3
heuristic_ready:
    add t6, t6, s3
    bltu s2, t6, search_backtrack
    or t6, t4, t5
    beqz t6, search_found

search_choice:
    beq s3, s2, search_backtrack
    # t0 points to next_stack[depth], and t1 is its current move (0..8).
    li t2, 9
    bgeu t1, t2, search_backtrack
    addi t2, t1, 1
    sb t2, 0(t0)

    # Two consecutive turns of the same face cannot be in a shortest path.
    beqz s3, search_apply
    la t3, move_face
    add t4, t3, t1
    lbu t4, 0(t4)
    addi t5, s3, -1
    add t5, s7, t5
    lbu t5, 0(t5)
    add t5, t3, t5
    lbu t5, 0(t5)
    beq t4, t5, search_node

search_apply:
    add t0, s7, s3
    sb t1, 0(t0)
    slli t0, t1, 2
    add t2, s10, t0
    lw t2, 0(t2)                  # permutation row pointer
    add t6, s11, t0
    lw t6, 0(t6)                  # orientation row pointer
    slli t3, s3, 1
    add t4, s4, t3
    lhu t5, 0(t4)
    slli t5, t5, 1
    add t2, t2, t5
    lhu t2, 0(t2)
    addi t4, t4, 2
    sh t2, 0(t4)
    add t4, s5, t3
    lhu t5, 0(t4)
    slli t5, t5, 1
    add t6, t6, t5
    lhu t6, 0(t6)
    addi t4, t4, 2
    sh t6, 0(t4)
    addi s3, s3, 1
    add t0, s6, s3
    sb zero, 0(t0)
    j search_node

search_backtrack:
    beqz s3, search_next_bound
    addi s3, s3, -1
    j search_node
search_next_bound:
    addi s2, s2, 1
    li t0, 12
    blt s2, t0, search_bound
    li a0, 1
    j exit_program

search_found:
    # Replay the returned path against the transition tables before printing.
    mv t0, s0
    mv t1, s1
    li t2, 0
verify_path:
    beq t2, s3, verify_done
    add t3, s7, t2
    lbu t3, 0(t3)
    slli t3, t3, 2
    add t4, s10, t3
    lw t4, 0(t4)
    slli t5, t0, 1
    add t4, t4, t5
    lhu t0, 0(t4)
    add t4, s11, t3
    lw t4, 0(t4)
    slli t5, t1, 1
    add t4, t4, t5
    lhu t1, 0(t4)
    addi t2, t2, 1
    j verify_path
verify_done:
    or t0, t0, t1
    bnez t0, invalid_output

    #if RENDER
    jal ra, draw_net                # Show the input before the first move.
    #endif
    li s0, 0                      # output index
    mv s1, s3                     # solution length
print_move:
    beq s0, s1, print_newline
    beqz s0, print_name
    li a0, 32
    li a7, 11
    ecall
print_name:
    add t0, s7, s0
    lbu t0, 0(t0)
    la t1, move_first
    add t1, t1, t0
    lbu a0, 0(t1)
    li a7, 11
    ecall
    la t1, move_second
    add t1, t1, t0
    lbu a0, 0(t1)
    beqz a0, print_next
    li a7, 11
    ecall
print_next:
    #if RENDER
    add t0, s7, s0
    lbu a0, 0(t0)
    jal ra, render_step             # Apply and show this emitted move.
    #endif
    addi s0, s0, 1
    j print_move
print_newline:
    li a0, 10
    li a7, 11
    ecall
    li a0, 0
    j exit_program

invalid_input:
    li a0, 2
    j exit_program
invalid_output:
    li a0, 1
exit_program:
    li a7, 93
    ecall

#if RENDER
# The visual state is the parsed input, never a prerecorded solution frame.
render_step:
    addi sp, sp, -16
    sw ra, 12(sp)
    la t0, move_face
    add t0, t0, a0
    lbu a4, 0(t0)                  # face
    la t0, move_turns
    add t0, t0, a0
    lbu a5, 0(t0)                  # quarter turns remaining
render_quarter:
    slli t0, a4, 3
    sub t0, t0, a4                # face * 7
    la t1, turn_source
    add t1, t1, t0
    la t2, turn_twist
    add t2, t2, t0
    la t3, pieces
    la t4, twists
    la a2, next_pieces
    la a3, next_twists
    li a6, 7
render_cubie:
    lbu t5, 0(t1)
    add t6, t3, t5
    lbu t6, 0(t6)
    sb t6, 0(a2)
    add t6, t4, t5
    lbu t6, 0(t6)
    lbu t5, 0(t2)
    add t6, t6, t5
    li t5, 3
    bltu t6, t5, render_twist_ready
    addi t6, t6, -3
render_twist_ready:
    sb t6, 0(a3)
    addi t1, t1, 1
    addi t2, t2, 1
    addi a2, a2, 1
    addi a3, a3, 1
    addi a6, a6, -1
    bnez a6, render_cubie
    la t1, next_pieces
    la t2, next_twists
    li a6, 7
render_copy:
    lbu t5, 0(t1)
    sb t5, 0(t3)
    lbu t5, 0(t2)
    sb t5, 0(t4)
    addi t1, t1, 1
    addi t2, t2, 1
    addi t3, t3, 1
    addi t4, t4, 1
    addi a6, a6, -1
    bnez a6, render_copy
    addi a5, a5, -1
    bnez a5, render_quarter
    jal ra, draw_net
    lw ra, 12(sp)
    addi sp, sp, 16
    ret

# Six faces × four stickers. facelet_pos and facelet_axis select a physical
# corner and one of its three cyclic sticker axes; cubie_axes gives its color.
draw_net:
    li a0, 0                      # sticker index, 0..23
    la a1, facelet_pos
    la a2, facelet_axis
    la a3, face_origin_x
    la a4, face_origin_y
    la a5, cube_colors
    li a6, LED_MATRIX_0_BASE
    li a7, LED_MATRIX_0_WIDTH
    slli a7, a7, 2                # byte stride, row-major
draw_sticker:
    add t0, a1, a0
    lbu t0, 0(t0)                 # physical position 0..7
    li t1, 0                      # fixed corner has orientation zero
    beqz t0, draw_fixed
    addi t0, t0, -1
    la t2, twists
    add t2, t2, t0
    lbu t1, 0(t2)
    la t2, pieces
    add t2, t2, t0
    lbu t0, 0(t2)
    addi t0, t0, 1                # cubie number 1..7
draw_fixed:
    add t2, a2, a0
    lbu t2, 0(t2)
    sub t2, t2, t1                # twist rotates the three stickers
    bgez t2, draw_axis_ready
    addi t2, t2, 3
draw_axis_ready:
    slli t1, t0, 1
    add t1, t1, t0                # cubie * 3
    add t1, t1, t2
    la t2, cubie_axes
    add t2, t2, t1
    lbu t2, 0(t2)                 # color index 0..5
    slli t2, t2, 2
    add t2, a5, t2
    lw t4, 0(t2)                  # RGB value

    srli t0, a0, 2               # face slot 0..5
    add t1, a3, t0
    lbu t1, 0(t1)                 # x origin
    add t2, a4, t0
    lbu t2, 0(t2)                 # y origin
    andi t0, a0, 3               # sticker within face
    andi t3, t0, 1
    slli t3, t3, 2
    add t1, t1, t3                # x
    srli t3, t0, 1
    slli t0, t3, 1
    add t3, t3, t0
    add t2, t2, t3                # y
    mv t5, a6                    # base + y * row stride + x * 4
draw_seek_row:
    beqz t2, draw_row_ready
    add t5, t5, a7
    addi t2, t2, -1
    j draw_seek_row
draw_row_ready:
    slli t1, t1, 2
    add t5, t5, t1
    li t3, 3
draw_pixel_row:
    mv t6, t5
    li t2, 4
draw_pixel:
    sw t4, 0(t6)
    addi t6, t6, 4
    addi t2, t2, -1
    bnez t2, draw_pixel
    add t5, t5, a7
    addi t3, t3, -1
    bnez t3, draw_pixel_row
    addi a0, a0, 1
    li t0, 24
    blt a0, t0, draw_sticker
    li t0, 25000                 # visible pause only in the GUI build
draw_pause:
    addi t0, t0, -1
    bnez t0, draw_pause
    ret
#endif

.data
input_state: .asciz "21345671111111"
pieces: .byte 0, 0, 0, 0, 0, 0, 0
twists: .byte 0, 0, 0, 0, 0, 0, 0
    .align 1
p_stack: .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
o_stack: .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
next_stack: .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
path: .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
move_face: .byte 0, 0, 0, 1, 1, 1, 2, 2, 2
move_first: .byte 82, 82, 82, 66, 66, 66, 68, 68, 68
move_second: .byte 0, 50, 39, 0, 50, 39, 0, 50, 39
#if RENDER
move_turns: .byte 1, 2, 3, 1, 2, 3, 1, 2, 3
turn_source:
    .byte 1, 4, 2, 0, 3, 5, 6
    .byte 0, 1, 2, 4, 5, 6, 3
    .byte 0, 2, 5, 3, 1, 4, 6
turn_twist:
    .byte 1, 2, 0, 2, 1, 0, 0
    .byte 0, 0, 0, 1, 2, 1, 2
    .byte 0, 0, 0, 0, 0, 0, 0
next_pieces: .byte 0, 0, 0, 0, 0, 0, 0
next_twists: .byte 0, 0, 0, 0, 0, 0, 0
# Face order: Up, Left, Front, Right, Back, Down.
face_origin_x: .byte 9, 0, 9, 18, 27, 9
face_origin_y: .byte 0, 7, 7, 7, 7, 14
facelet_pos:
    .byte 7, 4, 0, 1, 7, 0, 6, 3
    .byte 0, 1, 3, 2, 1, 4, 2, 5
    .byte 4, 7, 5, 6, 3, 2, 6, 5
facelet_axis:
    .byte 0, 0, 0, 0, 2, 1, 1, 2
    .byte 2, 1, 1, 2, 2, 1, 1, 2
    .byte 2, 1, 1, 2, 0, 0, 0, 0
# At twist zero, each corner's three stickers match its home faces.
# Cyclic order was checked against all three quarter-turn maps in solver.c.
cubie_axes:
    .byte 0, 1, 2, 0, 2, 3, 5, 3, 2, 5, 2, 1
    .byte 0, 3, 4, 5, 4, 3, 5, 1, 4, 0, 4, 1
    .align 2
cube_colors:
    .word 0xFFFFFF, 0xFF8000, 0x00FF00
    .word 0xFF0000, 0x0000FF, 0xFFFF00
#endif
