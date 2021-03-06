import threading
# ----------------------------------------
import HKSDK
# ----------------------------------------
# --------------------------------------------------


class Device(object):

    def __init__(self, camera_id: str,
                 host: str, port: int = 8000,
                 user: str = None, passwd: str = None):
        self._id = camera_id
        self._hostname = host
        self._port = port
        self._username = user
        self._password = passwd
        # ------------------------------
        self._user_id = -1
        self._handle_id = -1
        self._lock = threading.Lock()
        # ----------------------------------------
        ret_flag, status_code = HKSDK.init()
        if not ret_flag:
            raise OSError(f'Failed to initialize SDK -> StatusCode: {status_code}')

    def login(self) -> tuple:
        with self._lock:
            if self._user_id >= 0:
                return True, 0
            # ----------------------------------------
            user_id, status_code = HKSDK.login(
                self._hostname, self._username, self._password)
            if user_id >= 0:
                self._user_id = user_id
                return True, 0
            # ----------------------------------------
            return False, status_code

    def logout(self):
        with self._lock:
            if self._handle_id >= 0:
                raise ValueError(f'Failed to logout: Need close stream !')
            # ----------------------------------------
            if self._user_id < 0:
                return True, 0
            # ----------------------------------------
            ret_flag, status_code = HKSDK.logout(self._user_id)
            if ret_flag:
                self._user_id = -1
                return True, 0
            # ----------------------------------------
            return False, status_code

    def open(self):
        with self._lock:
            if self._user_id < 0:
                raise ValueError(f'Failed open stream: Need login !')
            # ----------------------------------------
            if self._handle_id >= 0:
                return True, 0
            # ----------------------------------------
            handle_id, status_code = HKSDK.open(self._user_id)
            if handle_id >= 0:
                self._handle_id = handle_id
                return True, 0
            # ----------------------------------------
            return False, status_code

    def close(self):
        with self._lock:
            if self._handle_id < 0:
                return True, 0
            # ----------------------------------------
            ret_flag, status_code = HKSDK.close(self._handle_id)
            if ret_flag:
                self._handle_id = -1
                return True, 0
            # ----------------------------------------
            return False, status_code

    def read(self):
        if self._handle_id < 0:
            return False, None
        # ----------------------------------------
        ret_flag, frame = HKSDK.get_frame(self._handle_id)
        if ret_flag:
            return True, frame
        # ----------------------------------------
        return False, None

    @classmethod
    def from_uri(cls, uri):
        pass

    @property
    def host(self):
        return self._hostname

    @property
    def id(self):
        return self._id

