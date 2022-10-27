import json
import re


def get_line_key(line):
    """Get key from line or return None if not found.
    EG from 'VectorParameterValues[1] = \n' return 'VectorParameterValues[1]',
    from 'ParameterName = Emissive Color' return 'ParameterName'
    """
    res = re.search(r"(?P<key>[a-zA-Z\d\[\]\s]+)=", line)
    if not res:
        return None
    else:
        return res.group("key").strip()


def get_text_until_closing_bracket(lines, lines_starting_index):
    """
    Get text from first opening bracket to corresponding closing bracket, return line numbers for this text block
    """
    text = ""
    bracket_detected = False
    bracket_counter = 0
    lines_to_skip = []

    for i, line in enumerate(lines):
        for char in line:
            if char == "{":
                bracket_detected = True
                bracket_counter += 1
            if char == "}":
                bracket_counter -= 1
            text += char
            if bracket_detected and bracket_counter == 0:
                lines_to_skip.append(lines_starting_index + i)
                return text, lines_to_skip
        lines_to_skip.append(lines_starting_index + i)
        text += "\n"
    raise ValueError("Bracket not closed")


def strip_brackets_from_string_once_if_needed(string):
    """
    Convert string '  { add: {badad: 1313}   }  ' to 'add: {badad: 1313}',
    raise error if string is '  { add: {badad: 1313}  ' (missing opening or closing bracket)
    """
    new_string = string.strip()
    opening_bracket = False
    closing_bracket = False

    # Forward pass
    for i, char in enumerate(new_string):
        if char == "{":
            opening_bracket = True
            new_string = new_string[i + 1:]
            break
    # Backward pass
    for i, char in enumerate(new_string[::-1]):
        if char == "}":
            closing_bracket = True
            new_string = new_string[:len(new_string) - i - 1]
            break
    if not closing_bracket and opening_bracket:
        raise ValueError("Closing bracket missing", string)
    if not opening_bracket and closing_bracket:
        raise ValueError("Opening bracket missing", string)
    else:
        return new_string


def boolify_nullify(s):
    if s.lower() in ['true']:
        return True
    if s.lower() in ['false']:
        return False
    if s.lower() in ['none', 'null']:
        return None
    raise ValueError(f"Couldn't boolify-nullify {s}")


def auto_convert(s):
    for fn in (boolify_nullify, int, float):
        try:
            return fn(s)
        except ValueError:
            pass
    return s


def remove_index_from_key(string):
    return re.search(r"(?P<key>[a-zA-Z\d\s]+)", string).group("key")


def parse_inline_value(string):
    """
    Parse inline nested structure string, eg, '{ Name=None }' or '{ R=1, G=1, B=1, A=0 }'
    """
    data = {}
    if not string.strip():
        return data
    parts = string.split(",")
    for part in parts:
        key, value = part.split("=")
        key = key.strip()
        data[key] = auto_convert(value.strip())
    return data


def parse_props_txt_file_content(content):
    """
    Parse content similar to
    '''
    VectorParameterValues[1] =
    {
        VectorParameterValues[0] =
        {
            ParameterName = Emissive Color
            ParameterValue = { R=1, G=1, B=1, A=0 }
            ParameterInfo = { Name=None }
        }
    }
    Parent = Material3'/EternalCrusade/Content/Materials/Templates/M_Template.M_Template'
    BasePropertyOverrides =
    {
        bOverride_BlendMode = false
        BlendMode = BLEND_Opaque (0)
        bOverride_TwoSided = false
        TwoSided = false
    }
    FlattenedTexture = None
    '''
    """
    data = {}
    lines = content.split("\n")

    lines_to_skip = []
    for i, line in enumerate(lines):
        if i in lines_to_skip:
            continue
        key = get_line_key(line)
        if not key:
            continue
        value_raw = "=".join(line.split("=")[1:]).strip()
        if not value_raw:
            # Is nested structure, start from next line
            content, t_lines_to_skip = get_text_until_closing_bracket(lines[i + 1:], i + 1)
            # Don't iterate through lines that are inside nested block
            lines_to_skip += t_lines_to_skip

            content = strip_brackets_from_string_once_if_needed(content)
            value = parse_props_txt_file_content(content)
        else:
            if "{" in value_raw:
                # Still nested structure, but 1-line
                content = value_raw
                content = strip_brackets_from_string_once_if_needed(content)
                value = parse_inline_value(content)
            else:
                value = auto_convert(value_raw)
        data[key] = value

    return data


def parse_props_txt_file(filepath):
    with open(filepath, "r") as f:
        content = f.read()
    return parse_props_txt_file_content(content)


def convert_props_txt_file_to_json(filepath):
    data = parse_props_txt_file(filepath)
    return json.dumps(data, indent=4)


if __name__ == '__main__':
    data = convert_props_txt_file_to_json("example.props.txt")
    print(data)
    with open("example.props.json", "w") as f:
        f.write(data)
