from pathlib import Path
import importlib.util
import shutil
import subprocess
import pytest
from machines_mcp.codecs.disassembly import decode

ROOT = Path(__file__).resolve().parents[2]


def test_generated_decoder_drift():
    spec = importlib.util.spec_from_file_location('generate_disassembly', ROOT / 'mcp/tools/generate_disassembly.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    assert module.OUTPUT.read_text() == module.generated()


def test_models_truncation_bounds_and_branch_wrap():
    assert decode(bytes.fromhex('a9118d3412d0f9'), 0x1000, 'nmos')[-1]['text'] == 'BNE $1000'
    assert decode(b'\x80\xfe', 0xFFFF - 1, '65c02')[0]['text'] == 'BRA $FFFE'
    assert decode(b'\x80\xfe', 0, 'nmos')[0]['text'] == '.BYTE $80'
    assert decode(b'\x9c\x34\x12', 0, '65c02')[0]['text'] == 'STZ $1234'
    lines = decode(b'\x4c\xea', 0xFFFE, 'nmos')
    assert [line['text'] for line in lines] == ['.BYTE $4C', '.BYTE $EA']
    assert all(line['truncated'] and line['forced_byte'] for line in lines)
    assert len(decode(b'\xea' * 20, 0, 'nmos', 3)) == 3
    assert decode(b'', 0, 'nmos') == []
    for data, address, cpu, limit in [(b'\xea\xea',65535,'nmos',1),(b'\xea',0,'bad',1),(b'',0,'nmos',0),(b'\xea'*769,0,'nmos',1)]:
        with pytest.raises(ValueError):
            decode(data,address,cpu,limit)


def test_every_opcode_matches_authoritative_c(tmp_path):
    """Compile only for development tests, never for adapter operation.

    Compare all opcode/model combinations with multiple operand bytes and
    addresses including relative target wrap. C's zero-filled truncated
    operand behavior is deliberately excluded; adapter tests cover .BYTE.
    """
    compiler = shutil.which('cc') or shutil.which('gcc') or shutil.which('clang')
    if not compiler:
        pytest.skip('C compiler unavailable for authoritative decoder parity')
    runner = tmp_path / 'parity.c'
    runner.write_text('''#include <stdio.h>
#include "disasm_6502.h"
int main(void) {
    unsigned addresses[] = {0, 0x1234, 0xfffd};
    unsigned operands[] = {0, 0x7f, 0x80, 0xff};
    for (int cpu=0;cpu<2;cpu++) for(int a=0;a<3;a++) for(int b=0;b<4;b++) for(int op=0;op<256;op++) {
        uint8_t bytes[3] = {op,operands[b],0x34};
        disasm_6502_line line=disasm_6502_decode_line(addresses[a],bytes,3,NULL,cpu);
        printf("%d|%u|%u|%d|%u|%d|%s\\n",cpu,addresses[a],operands[b],op,line.length,line.forced_byte,line.text);
    }
    return 0;
}''')
    source_dir = ROOT / 'src/shell/tools/disasm_6502'
    binary = tmp_path / 'parity'
    subprocess.run([compiler,'-std=c99','-I',str(source_dir),str(runner),str(source_dir/'disasm_6502.c'),'-o',str(binary)], check=True, capture_output=True)
    output = subprocess.run([str(binary)], check=True, capture_output=True, text=True).stdout
    for row in output.splitlines():
        cpu,address,operand,opcode,length,forced,text = row.split('|')
        line = decode(bytes((int(opcode),int(operand),0x34)),int(address),('nmos','65c02')[int(cpu)],1)[0]
        assert (line['length'],line['forced_byte'],line['text']) == (int(length),bool(int(forced)),text)
