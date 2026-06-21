import sys
import os
import unittest

# Append release build path to PYTHONPATH
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "build", "Release"))
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "build"))

import cones

class TestCoNESBindings(unittest.TestCase):
    def test_version(self):
        self.assertTrue(isinstance(cones.Version.string(), str))
        self.assertTrue(isinstance(cones.Version.full(), str))
        self.assertTrue(isinstance(cones.Version.MAJOR, int))

    def test_property_type_enum(self):
        self.assertEqual(int(cones.PropertyType.TEMPERATURE.value), int(cones.PropertyType.TEMPERATURE.value))
        self.assertEqual(cones.string_to_property("T"), cones.PropertyType.TEMPERATURE)
        self.assertEqual(cones.property_to_string(cones.PropertyType.TEMPERATURE), "Temperature")

    def test_dual_number(self):
        d1 = cones.DualNumber(2.0, 1.0)
        d2 = cones.DualNumber(3.0, 0.0)
        
        # Operators
        add = d1 + d2
        self.assertAlmostEqual(add.val, 5.0)
        self.assertAlmostEqual(add.der, 1.0)

        sub = d1 - d2
        self.assertAlmostEqual(sub.val, -1.0)
        self.assertAlmostEqual(sub.der, 1.0)

        mul = d1 * d2
        self.assertAlmostEqual(mul.val, 6.0)
        self.assertAlmostEqual(mul.der, 3.0)

        div = d1 / d2
        self.assertAlmostEqual(div.val, 2.0 / 3.0)
        self.assertAlmostEqual(div.der, 1.0 / 3.0)

        # Math functions
        self.assertAlmostEqual(cones.sin(d1).val, 0.90929742682)
        self.assertAlmostEqual(cones.cos(d1).val, -0.41614683654)
        self.assertAlmostEqual(cones.exp(d1).val, 7.38905609893)
        self.assertAlmostEqual(cones.log(d1).val, 0.69314718056)
        self.assertAlmostEqual(cones.sqrt(d1).val, 1.41421356237)

    def test_unit(self):
        u1 = cones.Unit.Pascal()
        self.assertEqual(u1.to_string(), "Pa")
        self.assertFalse(u1.is_dimensionless())
        self.assertTrue(u1.requires_positivity())

        u2 = cones.Unit.Celsius()
        self.assertEqual(u2.to_string(), "C")
        self.assertAlmostEqual(u2.offset, 273.15)

        # Operations
        u3 = u1 * u2
        self.assertAlmostEqual(u3.offset, 0.0)

    def test_unit_registry(self):
        reg = cones.UnitRegistry()
        names = reg.get_all_names()
        self.assertIn("Pa", names)
        self.assertIn("K", names)
        
        defn = reg.get("psia")
        self.assertIsNotNone(defn)
        self.assertEqual(defn.name, "psia")
        self.assertTrue(isinstance(defn.description, str))

    def test_variable_registry(self):
        reg = cones.VariableRegistry()
        idx = reg.register_variable("x_test", 10)
        self.assertEqual(idx, 0)
        self.assertEqual(reg.size(), 1)
        
        reg.set_value(idx, 15.0)
        reg.set_bounds(idx, 0.0, 100.0)
        reg.set_fixed(idx, False)

        var = reg.get_variable(idx)
        self.assertEqual(var.name, "x_test")
        self.assertAlmostEqual(var.value, 15.0)
        self.assertAlmostEqual(var.lower_bound, 0.0)
        self.assertAlmostEqual(var.upper_bound, 100.0)
        self.assertFalse(var.is_fixed)
        self.assertEqual(var.line, 10)

        # Get by name
        var2 = reg.get_variable("x_test")
        self.assertEqual(var2.index, idx)

    def test_constant_registry(self):
        reg = cones.ConstantRegistry()
        reg.load_standard_constants()
        self.assertIn("CONST_PI", reg.get_constant_names())
        c = reg.get("CONST_PI")
        self.assertIsNotNone(c)
        self.assertAlmostEqual(c.value, 3.141592653589793)

    def test_substance_manager(self):
        mgr = cones.SubstanceManager()
        mgr.register_ideal_gasses()
        self.assertIn("Air", mgr.get_substance_names())
        
        sub = mgr.get("Air")
        self.assertIsNotNone(sub)
        self.assertEqual(sub.name(), "Air")
        self.assertTrue("Ideal Gas" in sub.summary())

    def test_system_solve(self):
        sys_inst = cones.System()
        sys_inst.constant_registry().load_standard_constants()
        sys_inst.substance_manager().register_ideal_gasses()
        cones.register_builtin_functions(sys_inst.function_registry(), sys_inst.substance_manager())

        script = "T = 300 [K]\nP = 100000 [Pa]"
        lexer = cones.Lexer(script)
        tokens = lexer.scan_tokens()
        parser = cones.Parser(tokens, sys_inst, ".")
        parser.parse()

        solver = cones.NewtonSolver(1e-9, 100, False)
        report = solver.solve(sys_inst)
        self.assertTrue(report.success)

        # Test System.evaluate returning pair(f, j)
        f, j = sys_inst.evaluate()
        self.assertEqual(f.shape[0], sys_inst.get_equation_count())

if __name__ == "__main__":
    unittest.main()
