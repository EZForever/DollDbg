; --- External declarations (NOTE: Keep sync with hookll.h)
    
    ; Hook descriptor
hook_t struct 4
    va dq ?
    llstub dq ?
    trampoline dq ?
hook_t ends

    ; HookLLOnHook proto pHook:ptr hook_t, pEvent: ptr dword, isReturning: qword
extern HookLLOnHook: proto
    ; HookLLReturnStackPush proto value: qword
extern HookLLReturnStackPush: proto
    ; HookLLReturnStackPop proto
extern HookLLReturnStackPop: proto

; --- Public declarations

public HookLLCallStub
public HookLLReturnStub

public HookLLCallStub_oHookPtr
public HookLLCallStub_Size

; --- Private declarations

    ; Volatile registers in Windows x64 ABI
    ; https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention#callercallee-saved-registers
    ; XXX: ST0~ST7?
_context_t struct 10h
    _rax dq ?
    _rcx dq ?
    _rdx dq ?
    _r8 dq ?
    _r9 dq ?
    _r10 dq ?
    _r11 dq ?
    _rflags dq ?
align xmmword
    _xmm0 xmmword ?
    _xmm1 xmmword ?
    _xmm2 xmmword ?
    _xmm3 xmmword ?
    _xmm4 xmmword ?
    _xmm5 xmmword ?
    _mxcsr dd ?
_context_t ends

; --- Constant data segment
.const

HookLLCallStub_oHookPtr:
    dq __HookLLCallStub_pHook - HookLLCallStub

HookLLCallStub_Size:
    dq __HookLLCallStub_End - HookLLCallStub

; --- Code segment
.code

    ; Saves volatile registers to a buffer allocated on parent-allocated stack
    ; Context:
    ;   RSP & 0fh == 08h
    ;   _context_t will be stored at [RSP + 28h]
align 10h
_HookLLSaveContext proc
    mov _context_t._rax[rsp + 28h], rax
    mov _context_t._rcx[rsp + 28h], rcx
    mov _context_t._rdx[rsp + 28h], rdx
    mov _context_t._r8[rsp + 28h], r8
    mov _context_t._r9[rsp + 28h], r9
    mov _context_t._r10[rsp + 28h], r10
    mov _context_t._r11[rsp + 28h], r11
    pushfq
    pop _context_t._rflags[rsp + 28h]
    vmovdqa _context_t._xmm0[rsp + 28h], xmm0
    vmovdqa _context_t._xmm1[rsp + 28h], xmm1
    vmovdqa _context_t._xmm2[rsp + 28h], xmm2
    vmovdqa _context_t._xmm3[rsp + 28h], xmm3
    vmovdqa _context_t._xmm4[rsp + 28h], xmm4
    vmovdqa _context_t._xmm5[rsp + 28h], xmm5
    vstmxcsr _context_t._mxcsr[rsp + 28h]
    ret
_HookLLSaveContext endp

    ; Restore volatile registers from the stack
    ; Context:
    ;   RSP & 0fh == 08h
    ;   _context_t stored at [RSP + 28h]
align 10h
_HookLLRestoreContext proc
    mov rax, _context_t._rax[rsp + 28h]
    mov rcx, _context_t._rcx[rsp + 28h]
    mov rdx, _context_t._rdx[rsp + 28h]
    mov r8, _context_t._r8[rsp + 28h]
    mov r9, _context_t._r9[rsp + 28h]
    mov r10, _context_t._r10[rsp + 28h]
    mov r11, _context_t._r11[rsp + 28h]
    push _context_t._rflags[rsp + 28h]
    popfq
    vmovdqa xmm0, _context_t._xmm0[rsp + 28h]
    vmovdqa xmm1, _context_t._xmm1[rsp + 28h]
    vmovdqa xmm2, _context_t._xmm2[rsp + 28h]
    vmovdqa xmm3, _context_t._xmm3[rsp + 28h]
    vmovdqa xmm4, _context_t._xmm4[rsp + 28h]
    vmovdqa xmm5, _context_t._xmm5[rsp + 28h]
    vldmxcsr _context_t._mxcsr[rsp + 28h]
    ret
_HookLLRestoreContext endp

    ; Trigger the silmevent and wait for DbgEng to attach
    ; Context:
    ;   RSP & 0fh == 08h
    ;   Low 32 bits of [RSP] is used as slimevent state
align 10h
_HookLLHit proc frame
    .endprolog
    lock not dword ptr [rsp] ; Trigger the slimevent
    jmp $
    ; Go: RIP = trampoline
    ; Break-on-Return: RSP -= 8; [RSP] = pHook; RIP = HookLLReturn
    ; Skip / after Break-on-Return: { RAX = returnValue }; RIP = [RSP]; RSP += 8; { RSP += stackBytes }
_HookLLHit endp

    ; Dynamic routine for hooks
    ; Context:
    ;   RSP & 0fh == 08h
align 10h
HookLLCallStub proc
    push qword ptr [__HookLLCallStub_pHook]
    push qword ptr [__pHookLLCall]
    ret
    
align 8
__pHookLLCall:
    dq _HookLLCall
__HookLLCallStub_pHook::
    dq 79AFDEEBAD689111h
__HookLLCallStub_End::
HookLLCallStub endp

    ; Static routine for hooks
    ; Context:
    ;   RSP & 0fh == 00h
    ;   Hook address at [RSP]
    ;   Return address at [RSP + 8]
align 10h
_HookLLCall proc frame
__StackSize = sizeof _context_t + 20h
    .allocstack 8
    lea rsp, [rsp - __StackSize]
    .allocstack __StackSize
    .endprolog

    call _HookLLSaveContext

    mov rcx, [rsp + __StackSize]
    lea rdx, [rsp + __StackSize + 8]
    xor r8, r8
    call HookLLOnHook
    test rax, rax
    mov rax, _HookLLHit
    jnz @f
    mov rax, [rsp + __StackSize]
    mov rax, hook_t.trampoline[rax]
@@:
    mov [rsp + __StackSize], rax

    call _HookLLRestoreContext

    ; NOTE: This `ret` takes the additional pointer (trampoline or _HookLLHit) on the stack
    lea rsp, [rsp + __StackSize]
    ret
_HookLLCall endp

    ; Static stub for supporting break-on-return scenarios
    ; Context:
    ;   RSP & 0fh == 00h
    ;   Hook address at [RSP]
    ;   Return address at [RSP + 8]
align 10h
HookLLReturnStub proc frame
__StackSize = sizeof _context_t + 20h
    .allocstack 8
    lea rsp, [rsp - __StackSize]
    .allocstack __StackSize
    .endprolog

    call _HookLLSaveContext

    mov rcx, [rsp + __StackSize]
    call HookLLReturnStackPush
    mov rcx, [rsp + __StackSize + 8]
    call HookLLReturnStackPush

    mov rax, _HookLLReturn
    mov [rsp + __StackSize + 8], rax

    mov rax, [rsp + __StackSize]
    mov rax, hook_t.trampoline[rax]
    mov [rsp + __StackSize], rax

    call _HookLLRestoreContext

    ; NOTE: This `ret` takes the additional pointer (trampoline) on the stack
    lea rsp, [rsp + __StackSize]
    ret
HookLLReturnStub endp

    ; Static routine for break-on-return hooks
    ; Context:
    ;   RSP & 0fh == 00h
    ;   Return address not on stack
    ;   Return stack content: return address, hook address
align 10h
_HookLLReturn proc frame
__StackSize = sizeof _context_t + 20h + 8 ; Additional 8 for stack alignment
    lea rsp, [rsp - __StackSize - 8] ; Additional 8 for filling return address
    .allocstack __StackSize
    .endprolog

    call _HookLLSaveContext

    call HookLLReturnStackPop
    mov [rsp + __StackSize], rax

    call HookLLReturnStackPop
    mov rcx, rax
    lea rdx, [rsp + __StackSize]
    or r8, 1
    call HookLLOnHook

    call _HookLLRestoreContext

    lea rsp, [rsp + __StackSize]
    jmp _HookLLHit
_HookLLReturn endp

end