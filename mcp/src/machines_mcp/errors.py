"""Domain failures retain honest operation-effect information."""
class AdapterError(Exception):
    def __init__(self, code, message, *, effect="none", retry_safe=False, **details):
        super().__init__(message)
        self.data = dict(code=code, message=message, operation_effect=effect, retry_safe=retry_safe, **details)
