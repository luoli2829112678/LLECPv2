PROGRAM Main
VAR
    (* 第一根轴在当前项目中的ID为0；不是物理从站号。 *)
    AxisId : DINT := 0;
    Run : BOOL := FALSE;
    Stroke : LREAL := 10.0;
    Velocity : LREAL := 1.0;
    Acceleration : LREAL := 100.0;
    Deceleration : LREAL := 100.0;
    Jerk : LREAL := 1000.0;
    Tolerance : LREAL := 0.01;
    DwellScans : DINT := 500;
    EnableTimeoutScans : DINT := 5000;
    MoveTimeoutScans : DINT := 60000;
    (* 当前CyclicTask周期为1ms，500扫描约0.5秒。 *)
    SequenceStep : DINT := 0;
    AxisState : DINT := 0;
    ErrorCode : DINT := 0;
    StopResult : DINT := 0;
    Fault : BOOL := FALSE;
    HomePosition : LREAL := 0.0;
    OutPosition : LREAL := 0.0;
    ActualPosition : LREAL := 0.0;
    WaitScans : DINT := 0;
    Cycles : DINT := 0;
END_VAR

AxisState := AXIS_READ_STATE(AxisId);
IF (AxisState >= 0) AND (AxisState <= 7) THEN
    ActualPosition := AXIS_GETACTPOS(AxisId);
END_IF;

IF NOT Run THEN
    (* 只在退出往返时发送一次停止，不自动去使能。 *)
    IF (SequenceStep <> 0) AND (AxisState >= 3) AND (AxisState <= 7) THEN
        StopResult := AXIS_STOP(AxisId);
    END_IF;
    SequenceStep := 0;
    WaitScans := 0;
    Fault := FALSE;
    ErrorCode := 0;
ELSE
    IF (AxisState < 0) OR (AxisState > 7) THEN
        ErrorCode := AxisState;
        Fault := TRUE;
    ELSIF AxisState = 1 THEN
        ErrorCode := 15097;
        Fault := TRUE;
    END_IF;

    IF NOT Fault THEN
        IF SequenceStep = 0 THEN
            IF (Stroke <= Tolerance) OR (Tolerance <= 0.0) OR
               (Velocity <= 0.0) OR (Acceleration <= 0.0) OR
               (Deceleration <= 0.0) OR (Jerk <= 0.0) THEN
                ErrorCode := -1;
                Fault := TRUE;
            ELSIF AxisState = 0 THEN
                ErrorCode := AXIS_ENABLE_RESULT(AxisId);
                IF ErrorCode = 0 THEN
                    SequenceStep := 10;
                    WaitScans := 0;
                ELSE
                    Fault := TRUE;
                END_IF;
            ELSIF AxisState = 3 THEN
                HomePosition := ActualPosition;
                OutPosition := HomePosition + Stroke;
                SequenceStep := 20;
            ELSE
                (* 不接管已经在执行的运动；等轴静止后再启动。 *)
                ErrorCode := 15097;
                Fault := TRUE;
            END_IF;
        ELSIF SequenceStep = 10 THEN
            WaitScans := WaitScans + 1;
            IF AxisState = 3 THEN
                HomePosition := ActualPosition;
                OutPosition := HomePosition + Stroke;
                SequenceStep := 20;
                WaitScans := 0;
            ELSIF WaitScans > EnableTimeoutScans THEN
                ErrorCode := -2;
                Fault := TRUE;
            END_IF;
        ELSIF SequenceStep = 20 THEN
            ErrorCode := AXIS_ABS_MOVE(AxisId, OutPosition, Velocity, Acceleration, Deceleration, Jerk);
            IF ErrorCode = 0 THEN
                SequenceStep := 30;
                WaitScans := 0;
            ELSE
                Fault := TRUE;
            END_IF;
        ELSIF SequenceStep = 30 THEN
            WaitScans := WaitScans + 1;
            IF (AxisState = 3) AND (ActualPosition >= OutPosition - Tolerance) AND
               (ActualPosition <= OutPosition + Tolerance) THEN
                SequenceStep := 40;
                WaitScans := 0;
            ELSIF WaitScans > MoveTimeoutScans THEN
                ErrorCode := -3;
                Fault := TRUE;
            END_IF;
        ELSIF SequenceStep = 40 THEN
            WaitScans := WaitScans + 1;
            IF WaitScans >= DwellScans THEN
                SequenceStep := 50;
            END_IF;
        ELSIF SequenceStep = 50 THEN
            ErrorCode := AXIS_ABS_MOVE(AxisId, HomePosition, Velocity, Acceleration, Deceleration, Jerk);
            IF ErrorCode = 0 THEN
                SequenceStep := 60;
                WaitScans := 0;
            ELSE
                Fault := TRUE;
            END_IF;
        ELSIF SequenceStep = 60 THEN
            WaitScans := WaitScans + 1;
            IF (AxisState = 3) AND (ActualPosition >= HomePosition - Tolerance) AND
               (ActualPosition <= HomePosition + Tolerance) THEN
                SequenceStep := 70;
                WaitScans := 0;
                Cycles := Cycles + 1;
            ELSIF WaitScans > MoveTimeoutScans THEN
                ErrorCode := -3;
                Fault := TRUE;
            END_IF;
        ELSIF SequenceStep = 70 THEN
            WaitScans := WaitScans + 1;
            IF WaitScans >= DwellScans THEN
                SequenceStep := 20;
            END_IF;
        END_IF;
    END_IF;

    (* 故障锁存，不自动复位或再次发运动；Run置FALSE后才清程序故障。 *)
    IF Fault AND (SequenceStep <> 900) THEN
        IF (AxisState >= 3) AND (AxisState <= 7) THEN
            StopResult := AXIS_STOP(AxisId);
        END_IF;
        SequenceStep := 900;
    END_IF;
END_IF;
END_PROGRAM
