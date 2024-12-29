.model flat, C

; --- External declarations (NOTE: Keep sync with hookll.h)
    
    ; Hook descriptor
hook_t struct 4
    va dd ?
    llstub dd ?
    trampoline dd ?
hook_t ends

HookLLOnHook proto pHook:ptr hook_t, pEvent: ptr dword, isReturning: dword
;extern HookLLOnHook: proto
HookLLReturnStackPush proto value: dword
;extern HookLLReturnStackPush: proto
HookLLReturnStackPop proto
;extern HookLLReturnStackPop: proto

; --- Public declarations

public HookLLCallStub
public HookLLReturnStub

public HookLLCallStub_oHookPtr
public HookLLCallStub_Size

; --- Private declarations

    ; Volatile registers in most x86 ABIs
    ; https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/x86-architecture#calling-conventions
    ; https://docs.microsoft.com/en-us/cpp/cpp/floating-point-coprocessor-and-calling-conventions
    ; https://www.agner.org/optimize/calling_conventions.pdf
_context_t struct 4
    _eax dd ?
    _ecx dd ?
    _edx dd ?
    _eflags dd ?
    _xmm0 xmmword ?
    _xmm1 xmmword ?
    _xmm2 xmmword ?
    _xmm3 xmmword ?
    _xmm4 xmmword ?
    _xmm5 xmmword ?
    _xmm6 xmmword ?
    _xmm7 xmmword ?
    _mxcsr dd ?
_context_t ends

; --- Constant data segment
.const

HookLLCallStub_oHookPtr \
    dd __HookLLCallStub_pHook - HookLLCallStub

HookLLCallStub_Size \
    dd __HookLLCallStub_End - HookLLCallStub

; --- Code segment
.code

    ; Saves volatile registers to a buffer allocated on parent-allocated stack
    ; Context:
    ;   _context_t will be stored at [ESP + 4]
align 4
_HookLLSaveContext proc
    mov _context_t._eax[esp + 4], eax
    mov _context_t._ecx[esp + 4], ecx
    mov _context_t._edx[esp + 4], edx
    pushfd
    pop _context_t._eflags[esp + 4]
    vmovdqu _context_t._xmm0[esp + 4], xmm0
    vmovdqu _context_t._xmm1[esp + 4], xmm1
    vmovdqu _context_t._xmm2[esp + 4], xmm2
    vmovdqu _context_t._xmm3[esp + 4], xmm3
    vmovdqu _context_t._xmm4[esp + 4], xmm4
    vmovdqu _context_t._xmm5[esp + 4], xmm5
    vmovdqu _context_t._xmm6[esp + 4], xmm6
    vmovdqu _context_t._xmm7[esp + 4], xmm7
    vstmxcsr _context_t._mxcsr[esp + 4]
    ret
_HookLLSaveContext endp

    ; Restore volatile registers from the stack
    ; Context:
    ;   _context_t stored at [ESP + 4]
align 4
_HookLLRestoreContext proc
    mov eax, _context_t._eax[esp + 4]
    mov ecx, _context_t._ecx[esp + 4]
    mov edx, _context_t._edx[esp + 4]
    push _context_t._eflags[esp + 4]
    popfd
    vmovdqu xmm0, _context_t._xmm0[esp + 4]
    vmovdqu xmm1, _context_t._xmm1[esp + 4]
    vmovdqu xmm2, _context_t._xmm2[esp + 4]
    vmovdqu xmm3, _context_t._xmm3[esp + 4]
    vmovdqu xmm4, _context_t._xmm4[esp + 4]
    vmovdqu xmm5, _context_t._xmm5[esp + 4]
    vmovdqu xmm5, _context_t._xmm6[esp + 4]
    vmovdqu xmm5, _context_t._xmm7[esp + 4]
    vldmxcsr _context_t._mxcsr[esp + 4]
    ret
_HookLLRestoreContext endp

    ; Trigger the silmevent and wait for DbgEng to attach
    ; Context:
    ;   [ESP] is used as slimevent state
align 4
_HookLLHit proc
    lock not dword ptr [esp] ; Trigger the slimevent
    jmp $
    ; Go: EIP = trampoline
    ; Break-on-Return: ESP -= 4; [ESP] = pHook; EIP = HookLLReturn
    ; Skip / after Break-on-Return: { EAX = returnValue }; EIP = [ESP]; ESP += 8; { ESP += stackBytes }
_HookLLHit endp

    ; Dynamic routine for hooks
    ; Context:
    ;   (None)
align 4
HookLLCallStub proc
    db 68h ; push imm32
__HookLLCallStub_pHook::
    dd 91779111h
    push _HookLLCall
    ret
__HookLLCallStub_End::
HookLLCallStub endp

    ; Static routine for hooks
    ; Context:
    ;   Hook address at [ESP]
    ;   Return address at [ESP + 4]
align 4
_HookLLCall proc
    push ebp
    mov ebp, esp
    lea esp, [esp - sizeof _context_t]

    call _HookLLSaveContext

    lea eax, [ebp + 8]
    push 0
    push eax
    push dword ptr [ebp + 4]
    call HookLLOnHook
    add esp, 12
    test eax, eax
    mov eax, _HookLLHit
    jnz @f
    mov eax, [ebp + 4]
    mov eax, hook_t.trampoline[eax]
@@:
    mov [ebp + 4], eax

    call _HookLLRestoreContext

    ; NOTE: This `ret` takes the additional pointer (trampoline or _HookLLHit) on the stack
    leave
    ret
_HookLLCall endp

    ; Static stub for supporting break-on-return scenarios
    ; Context:
    ;   Hook address at [ESP]
    ;   Return address at [ESP + 4]
align 4
HookLLReturnStub proc
    push ebp
    mov ebp, esp
    lea esp, [esp - sizeof _context_t]

    call _HookLLSaveContext

    push dword ptr [ebp + 4]
    call HookLLReturnStackPush
    add esp, 4
    push dword ptr [ebp + 8]
    call HookLLReturnStackPush
    add esp, 4

    mov eax, _HookLLReturn
    mov [ebp + 8], eax

    mov eax, [ebp + 4]
    mov eax, hook_t.trampoline[eax]
    mov [ebp + 4], eax

    call _HookLLRestoreContext

    ; NOTE: This `ret` takes the additional pointer (trampoline) on the stack
    leave
    ret
HookLLReturnStub endp

    ; Static routine for break-on-return hooks
    ; Context:
    ;   Return address not on stack
    ;   Return stack content: return address, hook address
align 4
_HookLLReturn proc
    push eax ; Additional 4 for filling return address
    push ebp
    mov ebp, esp
    lea esp, [esp - sizeof _context_t]

    call _HookLLSaveContext

    call HookLLReturnStackPop
    lea ebx, [ebp + 4]
    mov [ebx], eax

    call HookLLReturnStackPop
    push 1
    push ebx
    push eax
    call HookLLOnHook
    add esp, 12

    call _HookLLRestoreContext

    leave
    jmp _HookLLHit
_HookLLReturn endp

end