<script setup>
import { ref } from "vue";

import RolekAlert from "./components/RolekAlert.vue";
import RolekFooter from "./components/RolekFooter.vue";
import ButtonGrid from "./components/ButtonGrid.vue";
import ResetButton from "./components/ResetButton.vue";
import RolekButton from "./components/RolekButton.vue";

const request_in_progress = ref(false);
const alert = ref(null);

const show_alert = (text) => {
  alert.value.set(text);
};
</script>

<template>
  <RolekAlert ref="alert" />

  <div class="container">
    <div class="row"></div>

    <div class="row justify-content-center mt-5">
      <div class="col-12 col-md-4">
        <h1 class="display-4 text-center">Rolek</h1>
      </div>
    </div>

    <div class="row justify-content-center mb-5">
      <RolekButton
        :large="true"
        :disabled="request_in_progress"
        @request_start="request_in_progress = true"
        @request_failure="show_alert('Request failed.')"
        @request_end="request_in_progress = false"
      />
    </div>

    <ButtonGrid
      :disabled="request_in_progress"
      @request_start="request_in_progress = true"
      @request_failure="show_alert('Request failed.')"
      @request_end="request_in_progress = false"
    />

    <div class="row justify-content-center m-5">
      <ResetButton
        :disabled="request_in_progress"
        @request_start="request_in_progress = true"
        @request_failure="show_alert('Request failed.')"
        @request_end="request_in_progress = false"
      />
    </div>

    <RolekFooter />
  </div>
</template>
