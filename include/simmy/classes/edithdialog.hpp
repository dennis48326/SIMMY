// @class EdithDialogPrimitive after UIDialog
// The behaviour trees' dialogs (game/edithdialogprim.cpp; primitive 0x24 TryDialog in objectsim.cpp).
// A DialogParam (8 operand bytes): +0 the third button's line, +1 the icon, +2 the message, +3 / +4 the
// first / second button's lines, +5 the kind (high nibble: 0 modal by its buttons, 1 / 2 modeless (2:
// 60 s), 3 modal with one button, 4 modal yes/no, 5 the intro thought balloon) and the players (low
// nibble: 1 player 1, 2 player 2, else both), +6 the title, +7 flags (0x80 the semi-global file's
// strings, bits 1-3 the icon kind: 2 a neighbour's, 3 an object's, 4 DialogIconTable; bits 4-6 the temp
// that gets the answer). Lines are 1-based in the tree file's string set 0x12D (0: none, 0xFE: empty);
// a temp's value names a line when the operand is -1 (cXObjectImpl::RewriteDialogParamIndices).
#pragma once

struct EdithDialogPrimitive : public UIDialog {
  EdithDialogPrimitive();  // 800B51EC
  ~EdithDialogPrimitive() override;  // 800B5480
  void SelectionCallback(int selection) override;  // 800B54B0
  // Line `index` of `strings` (else `fallback`) into `out`.
  void AssignString(const unsigned char* strings, std::vector<unsigned short>& out, int index,
                    const unsigned short* fallback, bool);  // 800B54D4
  void ResetDialogParameters();  // 800B55D0
  // Shows the dialog of `param` for `object`'s tree at `frame`. 1: shown.
  int SetDialog(StackElem* frame, const unsigned char* param, cXObject* object, bool testing);  // 800B56DC
  int GetModalDialogResult() const { return m_result; }  // 800B5F20

  int m_result = 0;  // +0x88 the button chosen
};

// DAT_80475F0C (the EORGlobals' dialog). PC: one, made on first use.
EdithDialogPrimitive* TheEdithDialog();
