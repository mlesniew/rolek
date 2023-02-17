<script setup>
import { ref } from "vue";

const text = ref(null);
const timeout = ref(null);

const dismiss = () => {
  text.value = null;
  if (timeout.value) {
    clearTimeout(timeout.value);
    timeout.value = null;
  }
};

const set = (new_text) => {
  dismiss();
  text.value = new_text;
  timeout.value = setTimeout(() => {
    dismiss();
  }, 5000);
};

defineExpose({ set });
</script>

<template>
  <div
    v-if="text"
    class="alert alert-danger alert-dismissible position-fixed rolek-alert"
    role="alert"
  >
    {{ text }}
    <button type="button" class="btn-close" @click="dismiss"></button>
  </div>
</template>

<style>
.rolek-alert {
  position: fixed;
  top: 10px;
  left: 10%;
  width: 80%;
  z-index: 9999;
}
</style>
