meta:
  id: qnd
  file-extension: qnd
  endian: le

types:
  sample:
    seq:
    - id: tag
      type: u2
#     valid:
#       eq: 0xFF00
    - id: len
      type: u2
    - id: tick
      type: u8
    - id: stacks
      type: stack
      repeat: expr
      repeat-expr: len
  stack:
    seq:
      - id: tag
        type: u2
#       valid:
#         eq: 0xEE00
      - id: len
        type: s2
      - id: tid
        type: u8
      - id: frames
        type: frame
        repeat: expr
        repeat-expr: len
  frame:
    seq:
      - id: tick
        type: u8
      - id: code
        type: u8

seq:
  - id: head
    type: u4
    valid:
      eq: 0x70646E71
  - id: tick_mul
    type: u8
  - id: tick_div
    type: u8
  - id: samples
    type: sample
    repeat: eos
