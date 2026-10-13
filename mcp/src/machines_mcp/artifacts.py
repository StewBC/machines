"""Bounded atomic artifact writes and immutable presentation pages."""
import hashlib
import json
import os
import tempfile
import time
import uuid
from collections import OrderedDict
from .errors import AdapterError

class Artifacts:
    def __init__(self, config):
        self.config = config
        self.pages = OrderedDict()

    def clear(self):
        self.pages.clear()

    def write(self, body, fmt, *, path=None, overwrite=False):
        if path is None:
            if self.config.artifact_dir is None:
                raise AdapterError('configuration', 'Configure artifact_dir for large results')
            path = str(self.config.artifact_dir / f'{uuid.uuid4().hex}.{fmt}')
        target = self.config.path(path, write=True, overwrite=overwrite)
        base = self.config.artifact_dir
        used = sum(p.stat().st_size for p in base.iterdir() if p.is_file()) if base else 0
        if used + len(body) > self.config.artifact_quota:
            raise AdapterError('artifact_quota', 'Artifact quota exceeded; remove exports or increase quota')
        fd, tmp = tempfile.mkstemp(prefix='.machines-mcp-',dir=target.parent)
        try:
            with os.fdopen(fd,'wb') as f:
                f.write(body)
                f.flush()
                os.fsync(f.fileno())
            if overwrite:
                os.replace(tmp,target)
            else:
                os.link(tmp,target) # exclusive publication; refuses a concurrent existing file
                os.unlink(tmp)
        finally:
            if os.path.exists(tmp):
                os.unlink(tmp)
        return dict(path=str(target), size=len(body), sha256=hashlib.sha256(body).hexdigest(),format=fmt)

    def bounded(self, data):
        raw = json.dumps(data,ensure_ascii=False).encode('utf-8')
        if len(raw) <= 60000:
            return data
        records = data.get('records', [])
        if not records:
            raise AdapterError('result_too_large','Result cannot fit text budget')
        token = uuid.uuid4().hex
        if len(raw)>16*1024*1024:
            raise AdapterError('result_too_large','Immutable page exceeds cache budget')
        self._expire()
        while self.pages and sum(p[2] for p in self.pages.values()) + len(raw)>16*1024*1024:
            self.pages.popitem(last=False)
        self.pages[token]=(time.monotonic(),json.loads(raw),len(raw))
        artifact = self.write(raw,'json')
        result = self.page(token,0,32)
        result['artifact']=artifact
        return result

    def _expire(self):
        now=time.monotonic()
        for token,(created,_,_) in list(self.pages.items()):
            if now-created>900:
                del self.pages[token]

    def page(self,token,offset,limit):
        if offset < 0 or not 1 <= limit <= 64:
            raise AdapterError('invalid_argument','Invalid immutable page offset/limit')
        self._expire()
        if token not in self.pages:
            raise AdapterError('expired','Presentation page expired or was evicted')
        data=self.pages[token][1]
        records=data['records']
        if offset>len(records):
            raise AdapterError('invalid_argument','Offset exceeds immutable page')
        result={k:v for k,v in data.items() if k!='records'}
        result.update(records=[],result_page_token=token,total_records=len(records),offset=offset)
        for record in records[offset:offset+limit]:
            if len(json.dumps(result).encode())+len(json.dumps(record).encode())>60000:
                break
            result['records'].append(record)
        if not result['records'] and offset<len(records):
            raise AdapterError('result_too_large','A single record exceeds text budget; use JSON artifact')
        next_offset=offset+len(result['records'])
        result['next_offset']=next_offset if next_offset<len(records) else None
        return result
