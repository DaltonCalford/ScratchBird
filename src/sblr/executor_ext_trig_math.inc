                    if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_COS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("COS expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            // P0-5: Check for NaN/Infinity
                            if (std::isnan(x))
                            {
                                error("COS argument cannot be NaN");
                            }
                            if (std::isinf(x))
                            {
                                error("COS argument cannot be Infinity");
                            }
                            push(Value::makeFloat64(std::cos(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_TAN))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("TAN expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            // P0-5: Check for NaN/Infinity
                            if (std::isnan(x))
                            {
                                error("TAN argument cannot be NaN");
                            }
                            if (std::isinf(x))
                            {
                                error("TAN argument cannot be Infinity");
                            }
                            push(Value::makeFloat64(std::tan(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_ASIN))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ASIN expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            if (x < -1.0 || x > 1.0)
                            {
                                error("ASIN argument must be in range [-1, 1]");
                            }
                            push(Value::makeFloat64(std::asin(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_ACOS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ACOS expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            if (x < -1.0 || x > 1.0)
                            {
                                error("ACOS argument must be in range [-1, 1]");
                            }
                            push(Value::makeFloat64(std::acos(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_ATAN))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ATAN expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            push(Value::makeFloat64(std::atan(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_ATAN2))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("ATAN2 expects 2 arguments, got " + std::to_string(arg_count));
                        }

                        // Pop in reverse order (x, then y)
                        Value x = pop();
                        Value y = pop();

                        if (y.isNull() || x.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            push(Value::makeFloat64(std::atan2(coerceToDouble(y), coerceToDouble(x))));
                        }
                    }
                    // Angle conversion functions
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_DEGREES))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("DEGREES expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double radians = coerceToDouble(arg);
                            push(Value::makeFloat64(radians * 180.0 / kPi));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_RADIANS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("RADIANS expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double degrees = coerceToDouble(arg);
                            push(Value::makeFloat64(degrees * kPi / 180.0));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_PI))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 0)
                        {
                            error("PI expects 0 arguments, got " + std::to_string(arg_count));
                        }

                        push(Value::makeFloat64(kPi));
                    }
                    // Algebraic functions
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_ABS))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ABS expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            // Handle different numeric types
                            if (arg.type() == core::DataType::INT64)
                            {
                                int64_t val = arg.toInt64();
                                push(Value::makeInt64(val < 0 ? -val : val));
                            }
                            else if (arg.type() == core::DataType::INT32)
                            {
                                int32_t val = arg.toInt64();
                                push(Value::makeInt32(val < 0 ? -val : val));
                            }
                            else
                            {
                                double val = coerceToDouble(arg);
                                push(Value::makeFloat64(std::abs(val)));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_SIGN))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("SIGN expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double val = coerceToDouble(arg);
                            int32_t sign = (val > 0.0) ? 1 : ((val < 0.0) ? -1 : 0);
                            push(Value::makeInt32(sign));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_ROUND))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count < 1 || arg_count > 2)
                        {
                            error("ROUND expects 1 or 2 arguments, got " + std::to_string(arg_count));
                        }

                        int32_t precision = 0;
                        if (arg_count == 2)
                        {
                            Value prec_val = pop();
                            if (!prec_val.isNull())
                            {
                                precision = prec_val.toInt64();
                            }
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double val = coerceToDouble(arg);
                            double multiplier = std::pow(10.0, precision);
                            push(Value::makeFloat64(std::round(val * multiplier) / multiplier));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_CEIL))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("CEIL expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double val = coerceToDouble(arg);
                            push(Value::makeFloat64(std::ceil(val)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_FLOOR))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("FLOOR expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double val = coerceToDouble(arg);
                            push(Value::makeFloat64(std::floor(val)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_TRUNC))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count < 1 || arg_count > 2)
                        {
                            error("TRUNC expects 1 or 2 arguments, got " + std::to_string(arg_count));
                        }

                        int32_t precision = 0;
                        if (arg_count == 2)
                        {
                            Value prec_val = pop();
                            if (!prec_val.isNull())
                            {
                                precision = prec_val.toInt64();
                            }
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double val = coerceToDouble(arg);
                            double multiplier = std::pow(10.0, precision);
                            push(Value::makeFloat64(std::trunc(val * multiplier) / multiplier));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_MOD))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 2)
                        {
                            error("MOD expects 2 arguments, got " + std::to_string(arg_count));
                        }

                        // Pop in reverse order (divisor, dividend)
                        Value divisor = pop();
                        Value dividend = pop();

                        if (dividend.isNull() || divisor.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double y = coerceToDouble(divisor);
                            if (y == 0.0)
                            {
                                error("Division by zero in MOD");
                            }
                            double x = coerceToDouble(dividend);
                            push(Value::makeFloat64(std::fmod(x, y)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_SQRT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("SQRT expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            // P0-5: Check for NaN/Infinity
                            if (std::isnan(x))
                            {
                                error("SQRT argument cannot be NaN");
                            }
                            if (std::isinf(x))
                            {
                                error("SQRT argument cannot be Infinity");
                            }
                            if (x < 0.0)
                            {
                                error("SQRT argument must be non-negative");
                            }
                            double result = std::sqrt(x);
                            // Defensive check: result should not be NaN or Inf for valid inputs
                            if (std::isnan(result) || std::isinf(result))
                            {
                                error("SQRT produced invalid result");
                            }
                            push(Value::makeFloat64(result));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_CBRT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("CBRT expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            push(Value::makeFloat64(std::cbrt(x)));
                        }
                    }
                    // Hyperbolic functions (Alpha 1 - Missing Functions)
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_SINH))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("SINH expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            push(Value::makeFloat64(std::sinh(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_COSH))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("COSH expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            push(Value::makeFloat64(std::cosh(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_TANH))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("TANH expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            push(Value::makeFloat64(std::tanh(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_ASINH))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ASINH expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            push(Value::makeFloat64(std::asinh(x)));
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_ACOSH))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ACOSH expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            if (x < 1.0)
                            {
                                // Domain error: ACOSH only defined for x >= 1
                                push(Value::makeNull());
                            }
                            else
                            {
                                push(Value::makeFloat64(std::acosh(x)));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_ATANH))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("ATANH expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            if (std::abs(x) >= 1.0)
                            {
                                // Domain error: ATANH only defined for |x| < 1
                                push(Value::makeNull());
                            }
                            else
                            {
                                push(Value::makeFloat64(std::atanh(x)));
                            }
                        }
                    }
                    else if (ext_op == static_cast<uint16_t>(ExtendedOpcode::EXT_FUNC_COT))
                    {
                        uint8_t arg_count = readByte();
                        if (arg_count != 1)
                        {
                            error("COT expects 1 argument, got " + std::to_string(arg_count));
                        }

                        Value arg = pop();
                        if (arg.isNull())
                        {
                            push(Value::makeNull());
                        }
                        else
                        {
                            double x = coerceToDouble(arg);
                            double tan_x = std::tan(x);
                            if (tan_x == 0.0)
                            {
                                // Division by zero - return NULL or infinity
                                push(Value::makeNull());
                            }
                            else
                            {
                                push(Value::makeFloat64(1.0 / tan_x));
                            }
                        }
                    }
