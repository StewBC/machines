"""Bounded disassembly using generated emulator opcode metadata.

Unknown opcodes use the shared decoder's .BYTE representation. Incomplete
instructions deliberately use .BYTE too: missing operands are never invented.
"""
from ._disassembly_opcodes import NMOS, CMOS_OVERRIDES


def decode(data: bytes, address: int, cpu_class: str, max_instructions: int = 64) -> list[dict]:
    if cpu_class not in ('nmos', '65c02'):
        raise ValueError('cpu_class must be nmos or 65c02')
    if isinstance(address, bool) or not isinstance(address, int) or not 0 <= address <= 65535:
        raise ValueError('address must be a 16-bit integer')
    if isinstance(max_instructions, bool) or not isinstance(max_instructions, int) or not 1 <= max_instructions <= 256:
        raise ValueError('max_instructions must be between 1 and 256')
    if not isinstance(data, bytes) or len(data) > 768 or address + len(data) > 65536:
        raise ValueError('data must be bytes, at most 768 bytes, without address wrap')
    lines = []
    offset = 0
    truncated_tail = False
    while offset < len(data) and len(lines) < max_instructions:
        opcode = data[offset]
        mnemonic, mode, length = CMOS_OVERRIDES.get(opcode, NMOS[opcode]) if cpu_class == '65c02' else NMOS[opcode]
        truncated_tail = truncated_tail or length > len(data) - offset
        forced = mnemonic is None or truncated_tail
        if forced:
            length = 1
            text = f'.BYTE ${opcode:02X}'
        else:
            raw = data[offset:offset + length]
            byte = raw[1] if length >= 2 else 0
            word = int.from_bytes(raw[1:3], 'little') if length >= 3 else 0
            operands = {
                'IMP': '', 'ACC': 'A', 'IMM': f'#${byte:02X}',
                'ZP': f'${byte:02X}', 'ZPX': f'${byte:02X},X', 'ZPY': f'${byte:02X},Y',
                'ABS': f'${word:04X}', 'ABSX': f'${word:04X},X', 'ABSY': f'${word:04X},Y',
                'IND': f'(${word:04X})', 'INDX': f'(${byte:02X},X)', 'INDY': f'(${byte:02X}),Y',
                'ZPIND': f'(${byte:02X})', 'ABSINDX': f'(${word:04X},X)',
                'REL': f'${(address + offset + 2 + (byte if byte < 128 else byte - 256)) & 65535:04X}',
            }
            operand = operands[mode]
            text = mnemonic + (' ' + operand if operand else '')
        lines.append({'address': address + offset, 'bytes_hex': data[offset:offset + length].hex(),
                      'length': length, 'text': text, 'forced_byte': forced, 'truncated': truncated_tail})
        offset += length
    return lines
