PROGRAM Main
VAR
  I : INT := 7;
  D : DINT;
  R : REAL := 2.5;
  L : LREAL;
  B : BOOL;
  Bits : WORD := 16#1234;
  U : UINT;
  RecordValue : Pair;
  ArrayValue : Numbers;
  RecordSize, ArraySize, TypeSize, FieldSize, ElementSize : ULINT;
  SmallSize : DINT;
  Out1, Out2, Mixed : LREAL;
  RoundEven, RoundOdd : INT;
  Block : SampleBlock;
  OK : BOOL;
END_VAR
D := I;
L := D;
B := I;
U := Bits;
RoundEven := R;
R := 3.5;
RoundOdd := R;
RecordSize := SIZEOF(RecordValue);
ArraySize := sizeof(ArrayValue);
TypeSize := SIZEOF(Pair);
FieldSize := SIZEOF(RecordValue.Large);
ElementSize := SIZEOF(ArrayValue[1]);
SmallSize := SIZEOF(INT);
Out1 := Echo(I);
Out2 := Echo(Value := I);
Block(Value := I);
Mixed := I + R;
OK := (D = 7) AND (L = 7.0) AND B AND (U = 16#1234)
  AND (RoundEven = 2) AND (RoundOdd = 4)
  AND (RecordSize = TypeSize) AND (ArraySize = 6)
  AND (FieldSize = 8) AND (ElementSize = 2) AND (SmallSize = 2)
  AND (Out1 = 7.0) AND (Out2 = 7.0) AND (Block.Result = 7.0) AND (Mixed = 10.5);
END_PROGRAM
