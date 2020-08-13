import sys
import os
# ----------------------------------------
from setuptools import setup, Extension
# --------------------------------------------------


PYTHON_HOME = os.path.abspath(os.path.dirname(os.path.dirname(sys.executable)))
BASE_SOURCE_DIR = os.path.abspath(os.path.dirname(__file__))


exts_hkws = Extension(
    name='HKSDK',
    language='c++',
    extra_compile_args=[
        '-std=c++11',
        '-pthread'
    ],
    include_dirs=[
        os.path.join(BASE_SOURCE_DIR, 'include'),
        os.path.join(PYTHON_HOME, 'include/python3.6m'),
        os.path.join(PYTHON_HOME, 'include')
    ],
    libraries=[
        'hcnetsdk', 'PlayCtrl',
        'opencv_shape', 'opencv_stitching', 'opencv_superres', 'opencv_videostab', 'opencv_aruco',
        'opencv_bgsegm', 'opencv_bioinspired', 'opencv_ccalib', 'opencv_datasets', 'opencv_dpm',
        'opencv_face', 'opencv_freetype', 'opencv_fuzzy', 'opencv_hdf', 'opencv_line_descriptor',
        'opencv_optflow', 'opencv_video', 'opencv_plot', 'opencv_reg','opencv_saliency', 'opencv_stereo',
        'opencv_structured_light', 'opencv_phase_unwrapping', 'opencv_rgbd','opencv_surface_matching',
        'opencv_text', 'opencv_ximgproc', 'opencv_calib3d', 'opencv_features2d', 'opencv_flann',
        'opencv_xobjdetect', 'opencv_objdetect', 'opencv_ml', 'opencv_xphoto', 'opencv_highgui',
        'opencv_videoio', 'opencv_imgcodecs', 'opencv_photo', 'opencv_imgproc', 'opencv_core'
    ],
    library_dirs=[
        # BASE_SOURCE_DIR,
        os.path.join(PYTHON_HOME, 'lib'),
        os.path.join(PYTHON_HOME, 'lib/hikvision'),
    ],
    sources=[
        'HKSDK.cpp'
    ]
)


setup(
    name='wai4demo',
    ext_modules=[
        exts_hkws
    ]
)
