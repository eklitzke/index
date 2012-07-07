import os

def get_root():
    return os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))

def get_py():
        return os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def get_static_dir():
    return os.path.join(get_root(), 'static')

def get_static_path(name):
    return os.path.join(get_static_dir(), name)

def get_template_dir():
    return os.path.join(get_static_dir(), 'mustache')

def get_template_path(name):
    return os.path.join(get_template_dir(), name)

def get_config_file():
    return os.path.join(get_py(), 'config.yaml')

def get_bin_path(name):
    return os.path.join(get_root(), 'bin', name)