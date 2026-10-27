#!/usr/bin/env python3
"""bmc_logger 单元测试：python3 -m unittest discover -s tools/droop-excite/tests"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import bmc_logger  # noqa: E402

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "fixtures", "sensor_list_0103.txt")


class TestParseSensorList(unittest.TestCase):
    def test_parses_real_0103_output(self):
        with open(FIXTURE) as f:
            vals = bmc_logger.parse_sensor_list(f.read())
        self.assertEqual(vals["CPU1 N_VDDAVS"], "0.910")
        self.assertEqual(vals["CPU2 N_VDDAVS"], "0.890")
        self.assertEqual(vals["CPU1 VDDAVS"], "0.890")
        self.assertEqual(vals["Power"], "270.000")
        self.assertEqual(vals["Power2"], "264.000")
        self.assertEqual(vals["PS2 IOut"], "19.980")
        self.assertEqual(vals["CPU1 Core Rem"], "63.000")

    def test_skips_na_and_malformed(self):
        text = ("Inlet Temp       | na         | degrees C  | na | x\n"
                "Bad line without pipes\n"
                "CPU1 Core Rem    | 63.000     | degrees C  | ok | x\n")
        vals = bmc_logger.parse_sensor_list(text)
        self.assertNotIn("Inlet Temp", vals)
        self.assertNotIn("Bad line without pipes", vals)
        self.assertEqual(vals["CPU1 Core Rem"], "63.000")

    def test_csv_columns_start_with_ts(self):
        self.assertEqual(bmc_logger.CSV_COLUMNS[0], "ts")
        for s in ("CPU1 N_VDDAVS", "CPU2 N_VDDAVS", "Power", "Power2"):
            self.assertIn(s, bmc_logger.CSV_COLUMNS)


if __name__ == "__main__":
    unittest.main()
