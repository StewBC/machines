import hashlib
import json
import pytest
from machines_mcp.config import Config
from machines_mcp.artifacts import Artifacts
from machines_mcp.errors import AdapterError


@pytest.fixture
def artifacts(tmp_path):
    return Artifacts(Config(read_roots=[tmp_path],write_roots=[tmp_path],artifact_dir=tmp_path))


def test_atomic_write_overwrite_and_hash(artifacts,tmp_path):
    path = str(tmp_path / 'spaces and ünicode.bin')
    result = artifacts.write(b'one','bin',path=path)
    assert result['sha256'] == hashlib.sha256(b'one').hexdigest()
    with pytest.raises(AdapterError) as error:
        artifacts.write(b'two','bin',path=path)
    assert error.value.data['code'] == 'file_exists'
    artifacts.write(b'two','bin',path=path,overwrite=True)
    assert (tmp_path / 'spaces and ünicode.bin').read_bytes() == b'two'
    assert not list(tmp_path.glob('.machines-mcp-*'))


def test_quota_is_explicit(artifacts):
    artifacts.config.artifact_quota = 3
    artifacts.write(b'abc','bin')
    with pytest.raises(AdapterError) as error:
        artifacts.write(b'd','bin')
    assert error.value.data['code'] == 'artifact_quota'


def test_paths_and_symlink_escape(artifacts,tmp_path):
    outside = tmp_path.parent / (tmp_path.name + '-outside')
    outside.mkdir()
    try:
        link = tmp_path / 'escape'
        try:
            link.symlink_to(outside,target_is_directory=True)
        except OSError:
            pytest.skip('host does not allow symlinks')
        with pytest.raises(AdapterError):
            artifacts.write(b'x','bin',path=str(link / 'x'))
        with pytest.raises(AdapterError):
            artifacts.write(b'x','bin',path=str(outside / 'x'))
        for path in ('relative.bin',str(tmp_path / 'bad\nname')):
            with pytest.raises(ValueError):
                artifacts.write(b'x','bin',path=path)
    finally:
        outside.rmdir()


def test_bounded_pages_expire_and_clear(artifacts):
    data = {'epoch':'1','records':[{'id':str(i),'text':'x'*3000} for i in range(40)]}
    first = artifacts.bounded(data)
    assert len(json.dumps(first).encode()) < 65536
    token = first['result_page_token']
    records = first['records'][:]
    offset = first['next_offset']
    while offset is not None:
        page = artifacts.page(token,offset,32)
        records.extend(page['records'])
        offset = page['next_offset']
    assert records == data['records']
    created,cached,size = artifacts.pages[token]
    artifacts.pages[token] = (created-901,cached,size)
    with pytest.raises(AdapterError) as error:
        artifacts.page(token,0,32)
    assert error.value.data['code'] == 'expired'
    artifacts.clear()
    assert not artifacts.pages


def test_existing_artifact_directory_configuration_loads(tmp_path):
    import json
    config = tmp_path / 'config.toml'
    config.write_text('write_roots = [' + json.dumps(str(tmp_path)) + ']\nartifact_dir = ' + json.dumps(str(tmp_path)) + '\n')
    assert Config.load(str(config)).artifact_dir == tmp_path


def test_pages_are_immutable_snapshots(artifacts):
    original = {'records':[{'id':str(i),'text':'x'*3000} for i in range(40)]}
    result = artifacts.bounded(original)
    original['records'][0]['id'] = 'mutated'
    assert artifacts.page(result['result_page_token'],0,1)['records'][0]['id'] == '0'
