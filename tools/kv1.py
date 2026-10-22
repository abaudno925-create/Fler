"""Valve KeyValues 1 parser — enough for CS2's items_game.txt."""


class KV1:
    def __init__(self):
        self.value = None
        self.children = {}

    def get(self, key, default=None):
        return self.children.get(key, default)

    def __getitem__(self, key):
        return self.children[key]


def parse(text):
    """Returns a dict of {key: value} where a value is str or KV1."""
    pos = 0
    length = len(text)

    def skip_ws():
        nonlocal pos
        while pos < length:
            ch = text[pos]
            if ch in ' \t\r\n':
                pos += 1
            elif text.startswith('//', pos):
                end = text.find('\n', pos)
                pos = length if end < 0 else end + 1
            elif text.startswith('/*', pos):
                end = text.find('*/', pos)
                pos = length if end < 0 else end + 2
            else:
                return

    def read_token():
        nonlocal pos
        skip_ws()
        if pos >= length:
            return None
        if text[pos] == '{':
            pos += 1
            return '{'
        if text[pos] == '}':
            pos += 1
            return '}'
        if text[pos] == '"':
            pos += 1
            out = []
            while pos < length:
                ch = text[pos]
                if ch == '\\' and pos + 1 < length:
                    nxt = text[pos + 1]
                    out.append({'n': '\n', 't': '\t', '"': '"', '\\': '\\'}.get(nxt, nxt))
                    pos += 2
                    continue
                if ch == '"':
                    pos += 1
                    break
                out.append(ch)
                pos += 1
            return ''.join(out)
        start = pos
        while pos < length and text[pos] not in ' \t\r\n{}':
            pos += 1
        return text[start:pos]

    def parse_block():
        node = KV1()
        while True:
            token = read_token()
            if token is None or token == '}':
                return node
            if token == '{':
                continue
            value = read_token()
            if value is None:
                return node
            if value == '{':
                block = parse_block()
                existing = node.children.get(token)
                if isinstance(existing, KV1):
                    # conditional blocks repeat sections — merge them
                    existing.children.update(block.children)
                else:
                    node.children[token] = block
            else:
                node.children[token] = value

    return parse_block()
