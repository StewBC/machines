def metadata(text: str) -> dict:
    result = {}
    for token in text.split():
        if '=' not in token:
            raise ValueError('invalid metadata token')
        key, value = token.split('=', 1)
        if not key or key in result:
            raise ValueError('duplicate or empty metadata key')
        result[key] = value
    return result


def integer(data: dict, key: str, maximum: int = 2**64 - 1) -> int:
    try:
        value = int(data[key], 10)
    except (KeyError, ValueError, TypeError) as exc:
        raise ValueError(f'invalid metadata {key}') from exc
    if not 0 <= value <= maximum:
        raise ValueError(f'out of range {key}')
    return value


def product_name(product: str) -> str:
    if product in ('apple2', 'a2m'):
        return 'apple2'
    if product in ('c64', 'c64m'):
        return 'c64'
    raise ValueError('unknown product')
