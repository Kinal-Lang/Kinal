// Closure environments contain shared storage addresses, never snapshots.
// Lower captured allocas only after all bodies have been emitted: changing a
// VarSymbol during expression emission misses lvalue handles already held by a
// caller, and promoting at closure construction breaks conditional dominance.

static void capture_record_storage(Codegen *g, LLVMValueRef address,
                                   LLVMBasicBlockRef block, LLVMValueRef previous)
{
    if (g->finalizing_capture_storage) return;
    CaptureStorage *storage = (CaptureStorage *)kn_malloc(sizeof(CaptureStorage));
    storage->address = address;
    storage->declaration_block = block;
    storage->declaration_previous = previous;
    storage->gc_frame = g->gc_frame;
    storage->captured = 0;
    storage->next = g->capture_storage;
    g->capture_storage = storage;
}

static void capture_mark_storage(Codegen *g, LLVMValueRef address)
{
    // An inherited capture already points into its producer's managed cell.
    if (!address || !LLVMIsAAllocaInst(address)) return;
    for (CaptureStorage *storage = g->capture_storage; storage; storage = storage->next)
    {
        if (storage->address == address)
        {
            storage->captured = 1;
            return;
        }
    }
    kn_die("captured local storage has no declaration point");
}

static void capture_position_after(Codegen *g, LLVMBasicBlockRef block, LLVMValueRef previous)
{
    LLVMValueRef next = previous ? LLVMGetNextInstruction(previous) : LLVMGetFirstInstruction(block);
    if (next) LLVMPositionBuilderBefore(g->builder, next);
    else LLVMPositionBuilderAtEnd(g->builder, block);
}

static void capture_root_raw_slot(Codegen *g, LLVMValueRef frame,
                                  LLVMValueRef slot, LLVMTypeRef type)
{
    LLVMValueRef size = LLVMSizeOf(type);
    if (LLVMTypeOf(size) != g->i64)
        size = LLVMBuildZExtOrBitCast(g->builder, size, g->i64, "capture.root.size");
    LLVMValueRef address = LLVMBuildBitCast(g->builder, slot, g->i8ptr, "capture.root.address");
    LLVMValueRef arguments[3] = {frame, address, size};
    LLVMBuildCall2(g->builder, g->fn_gc_add_root_ty, g->fn_gc_add_root, arguments, 3, "");
}

static void capture_promote_cell(Codegen *g, CaptureStorage *storage)
{
    LLVMValueRef old = storage->address;
    LLVMValueRef function = LLVMGetBasicBlockParent(storage->declaration_block);
    LLVMTypeRef type = LLVMGetAllocatedType(old);
    unsigned alignment = LLVMGetAlignment(old);
    unsigned abi_alignment = LLVMABIAlignmentOfType(g->target_data, type);
    if (alignment < abi_alignment) alignment = abi_alignment;
    if (!storage->gc_frame || !g->fn_gc_alloc || !g->fn_gc_add_root ||
        LLVMGetBasicBlockParent(LLVMGetInstructionParent(storage->gc_frame)) != function)
        kn_die("captured local storage requires its owning GC frame");

    LLVMValueRef root = build_entry_alloca(g, function, g->i8ptr, "capture.cell.root");

    // Allocation belongs to the declaration, not the closure expression or the
    // function entry. A loop-local declaration therefore gets a fresh cell on
    // each iteration; all closures over that instance still share one cell.
    capture_position_after(g, storage->declaration_block, storage->declaration_previous);
    LLVMValueRef bytes = LLVMSizeOf(type);
    if (LLVMTypeOf(bytes) != g->i64)
        bytes = LLVMBuildZExtOrBitCast(g->builder, bytes, g->i64, "capture.cell.size");
    if (alignment > 1)
        bytes = LLVMBuildAdd(g->builder, bytes, LLVMConstInt(g->i64, alignment - 1, 0), "capture.cell.capacity");
    LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc, &bytes, 1, "capture.cell.memory");
    LLVMValueRef cell = raw;
    if (alignment > 1)
    {
        LLVMTypeRef address_type = LLVMIntTypeInContext(g->ctx, (unsigned)g->target_pointer_bits);
        LLVMValueRef address = LLVMBuildPtrToInt(g->builder, raw, address_type, "capture.cell.address");
        address = LLVMBuildAdd(g->builder, address, LLVMConstInt(address_type, alignment - 1, 0), "capture.cell.rounded");
        address = LLVMBuildAnd(g->builder, address, LLVMConstInt(address_type, ~((uint64_t)alignment - 1), 0), "capture.cell.aligned");
        cell = LLVMBuildIntToPtr(g->builder, address, g->i8ptr, "capture.cell");
    }
    LLVMBuildStore(g->builder, cell, root);
    LLVMValueRef typed = LLVMBuildBitCast(g->builder, cell, LLVMTypeOf(old), "capture.cell.typed");

    // Register the root once, even for loop-local cells. Existing environments
    // retain older cells when the declaration updates this frame's root slot.
    LLVMBasicBlockRef frame_block = LLVMGetInstructionParent(storage->gc_frame);
    capture_position_after(g, frame_block, storage->gc_frame);
    LLVMBuildStore(g->builder, LLVMConstNull(g->i8ptr), root);
    capture_root_raw_slot(g, storage->gc_frame, root, g->i8ptr);

    // The managed cell is now scanned through its rooted pointer. Do not retain
    // the old stack-root registrations: a repeated declaration replaces the
    // cell, and such a registration would keep scanning a freed heap address.
    const char *marker = "kn.gc.entry.root.zero";
    unsigned zero_kind = LLVMGetMDKindIDInContext(g->ctx, marker, (unsigned)kn_strlen(marker));
    for (LLVMUseRef use = LLVMGetFirstUse(old); use; )
    {
        LLVMUseRef next = LLVMGetNextUse(use);
        LLVMValueRef user = LLVMGetUser(use);
        if (LLVMIsACallInst(user) && LLVMGetCalledValue(user) == g->fn_gc_add_root &&
            LLVMGetOperand(user, 1) == old)
            LLVMInstructionEraseFromParent(user);
        else if (LLVMIsAStoreInst(user) && LLVMGetMetadata(user, zero_kind))
            LLVMInstructionEraseFromParent(user);
        use = next;
    }

    LLVMReplaceAllUsesWith(old, typed);
    LLVMInstructionEraseFromParent(old);
}

static void capture_root_incoming_arguments(Codegen *g, LLVMValueRef frame)
{
    LLVMBasicBlockRef block = LLVMGetInstructionParent(frame);
    LLVMValueRef function = LLVMGetBasicBlockParent(block);
    capture_position_after(g, block, frame);
    // Promoted parameter cells can allocate before the ordinary parameter
    // stores. Root incoming managed representations first, including a lambda's
    // hidden environment, so collection cannot lose another incoming argument.
    unsigned count = LLVMCountParams(function);
    for (unsigned index = 0; index < count; index++)
    {
        LLVMValueRef parameter = LLVMGetParam(function, index);
        LLVMTypeRef type = LLVMTypeOf(parameter);
        LLVMTypeKind kind = LLVMGetTypeKind(type);
        if (kind != LLVMPointerTypeKind && kind != LLVMStructTypeKind && kind != LLVMArrayTypeKind)
            continue;
        LLVMValueRef root = build_entry_alloca(g, function, type, "capture.argument.root");
        LLVMBuildStore(g->builder, parameter, root);
        capture_root_raw_slot(g, frame, root, type);
    }
}

static void promote_captured_storage(Codegen *g)
{
    g->finalizing_capture_storage = 1;
    for (CaptureStorage *storage = g->capture_storage; storage; storage = storage->next)
        if (storage->captured) capture_promote_cell(g, storage);

    // Insert this prologue last so it precedes every promoted parameter cell.
    for (CaptureStorage *storage = g->capture_storage; storage; storage = storage->next)
    {
        if (!storage->captured) continue;
        int first = 1;
        for (CaptureStorage *previous = g->capture_storage; previous != storage; previous = previous->next)
            if (previous->captured && previous->gc_frame == storage->gc_frame) { first = 0; break; }
        if (first) capture_root_incoming_arguments(g, storage->gc_frame);
    }
    while (g->capture_storage)
    {
        CaptureStorage *next = g->capture_storage->next;
        kn_free(g->capture_storage);
        g->capture_storage = next;
    }
    g->finalizing_capture_storage = 0;
}
