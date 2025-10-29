meta:
  id: qnd
  file-extension: qnd
  endian: le

types:
  head:
    seq:
    - id: tag
      type: u4
      valid:
        eq: 0x70646E71
    - id: tick_mul
      type: u8
    - id: tick_div
      type: u8
  code:
    seq:
    - id: ptr
      type: u8
    - id: filename
      type: utf8
    - id: qualname
      type: utf8
    - id: line
      type: u2
  node:
    seq:
    - id: ptr
      type: u8
    - id: node_id
      type: u4
    - id: caller_id
      type: u4
  time:
    seq:
      - id: thread_id
        type: u8
      - id: node_id
        type: u4
      - id: time_active
        type: u8
      - id: time_paused
        type: u8
      - id: time_waited
        type: u8
  emit:
    seq:
      - id: time
        type: u8
  utf8:
    seq:
    - id: ptr
      type: u8
    - id: size
      type: u2
    - id: text
      type: str
      size: size
      encoding: UTF-8
  rec:
    seq:
    - id: tag
      type: u2
    - id: rec
      type:
        switch-on: tag
        cases:
          0xDEC0: code
          0xDED0: node
          0xFFFF: time
          0x1010: emit

seq:
  - id: head
    type: head
  - id: rec_list
    type: rec
    repeat: eos
