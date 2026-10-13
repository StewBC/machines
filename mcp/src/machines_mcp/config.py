"""Human-owned configuration and explicit path admission boundaries."""
from dataclasses import dataclass, field
from pathlib import Path
import tomllib
from .errors import AdapterError

@dataclass
class Config:
    read_roots: list[Path] = field(default_factory=list)
    write_roots: list[Path] = field(default_factory=list)
    artifact_dir: Path | None = None
    artifact_quota: int = 256 * 1024 * 1024
    profiles: dict = field(default_factory=dict)

    @classmethod
    def load(cls, path):
        p = Path(path)
        if not p.is_absolute():
            raise ValueError('configuration path must be absolute')
        with p.open('rb') as f:
            data = tomllib.load(f)
        roots = {}
        for name in ('read_roots', 'write_roots'):
            roots[name] = [_absolute(x) for x in data.get(name, [])]
        artifact = _absolute(data['artifact_dir']) if data.get('artifact_dir') else None
        cfg = cls(**roots, artifact_dir=artifact, artifact_quota=data.get('artifact_quota',256*1024*1024), profiles=data.get('profiles', {}))
        if cfg.artifact_quota < 1024:
            raise ValueError('artifact quota must be >=1024')
        if artifact:
            if not any(artifact.is_relative_to(root) for root in cfg.write_roots):
                raise ValueError('artifact_dir must be inside a write root')
            artifact.mkdir(parents=True, exist_ok=True)
        for profile in cfg.profiles.values():
            if profile['product'] not in ('apple2','c64'):
                raise ValueError('profile product must be apple2 or c64')
            for name in ('executable','cwd'):
                profile[name] = str(_absolute(profile[name]))
            if not Path(profile['executable']).is_file() or not Path(profile['cwd']).is_dir():
                raise ValueError('profile executable/cwd must exist')
            if profile.get('ini'):
                profile['ini'] = str(cfg.path(profile['ini']))
            for arg in profile.get('argv', []):
                if not isinstance(arg,str) or '\x00' in arg or arg.startswith('--control-port') or arg in ('--headless',):
                    raise ValueError('invalid fixed profile argv or managed lifecycle flag')
        return cfg

    def path(self, value, *, write=False, overwrite=False):
        p = _absolute(value)
        roots = self.write_roots if write else self.read_roots
        if not any(p.is_relative_to(root) for root in roots):
            raise AdapterError('path_denied', 'Path is outside configured roots; update local configuration')
        if write:
            if p.exists() and not overwrite:
                raise AdapterError('file_exists', 'Output exists; overwrite=true is required')
            if not p.parent.is_dir():
                raise AdapterError('path_denied', 'Output parent directory must exist')
        elif not p.exists():
            raise AdapterError('file_missing','Input path does not exist')
        return p

def _absolute(value):
    if any(c in str(value) for c in '\r\n\x00'):
        raise ValueError('Paths cannot contain CR, LF, or NUL')
    p = Path(value)
    if not p.is_absolute():
        raise ValueError('Paths must be absolute')
    return p.resolve()
